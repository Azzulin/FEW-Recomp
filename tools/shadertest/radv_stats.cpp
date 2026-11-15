// Compiles compute SPIR-V with the Vulkan driver at hand and prints the driver's statistics for each executable
// (VK_KHR_pipeline_executable_properties). Meant for the Steam Deck's compiler without a Deck: in WSL/Linux, Mesa's
// RADV on the amdgpu "noop" drm-shim (AMDGPU_GPU_ID=vangogh) compiles exactly like the Deck's GPU (VGPRs, SGPRs,
// spills, scratch, instruction count). Mesa 26 removed the older RADV_FORCE_FAMILY null device; the shim is built
// from Mesa's source with -Dtools=drm-shim (see tools/shadertest/wsl_bench.sh).
//
//   clang++ -std=c++20 -O1 tools/shadertest/radv_stats.cpp -lvulkan -o radv_stats
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/radeon_icd.json LD_PRELOAD=<libamdgpu_noop_drm_shim.so> //       AMDGPU_GPU_ID=vangogh ./radv_stats a.spv b.spv
//
// The descriptor set layout is rebuilt from the SPIR-V (DescriptorSet/Binding decorations and storage classes),
// so any program the translator emits can be compiled.
#include <vulkan/vulkan.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define CHECK(x)                                                         \
    do {                                                                 \
        VkResult r_ = (x);                                               \
        if (r_ != VK_SUCCESS) {                                          \
            fprintf(stderr, "%s failed: %d (line %d)\n", #x, r_, __LINE__); \
            exit(1);                                                     \
        }                                                                \
    } while (0)

static std::vector<uint32_t> read_spv(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint32_t> w(size_t(n) / 4);
    fread(w.data(), 4, w.size(), f);
    fclose(f);
    return w;
}

struct Binding {
    uint32_t set = 0, binding = 0, count = 1;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
};

// Minimal SPIR-V reflection: enough for the translator's output.
static std::vector<Binding> reflect(const std::vector<uint32_t>& w, uint32_t& push_size) {
    std::map<uint32_t, uint32_t> dset, dbind;
    std::map<uint32_t, bool> block, buffer_block;
    std::map<uint32_t, std::vector<uint32_t>> type;  // id -> instruction words
    std::map<uint32_t, uint32_t> constant;
    struct Var {
        uint32_t id, ptr_type, sc;
    };
    std::vector<Var> vars;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> member_offset;
    for (size_t i = 5; i < w.size();) {
        uint32_t op = w[i] & 0xFFFF, n = w[i] >> 16;
        if (!n) break;
        const uint32_t* a = &w[i + 1];
        switch (op) {
        case 71:  // OpDecorate
            if (a[1] == 34) dset[a[0]] = a[2];
            if (a[1] == 33) dbind[a[0]] = a[2];
            if (a[1] == 2) block[a[0]] = true;
            if (a[1] == 3) buffer_block[a[0]] = true;
            break;
        case 72:  // OpMemberDecorate
            if (a[2] == 35) member_offset[{a[0], a[1]}] = a[3];
            break;
        case 43:  // OpConstant
            constant[a[1]] = a[2];
            break;
        case 59:  // OpVariable
            vars.push_back({a[1], a[0], a[2]});
            break;
        default:
            if (op >= 19 && op <= 39) type[a[0]] = std::vector<uint32_t>(w.begin() + i, w.begin() + i + n);
            break;
        }
        i += n;
    }
    auto size_of = [&](auto& self, uint32_t t) -> uint32_t {
        auto& d = type[t];
        uint32_t op = d[0] & 0xFFFF;
        if (op == 21 || op == 22) return d[2] / 8;                                  // int, float
        if (op == 23) return d[3] * self(self, d[2]);                               // vector
        if (op == 28) return constant[d[3]] * self(self, d[2]);                     // array (no stride check)
        if (op == 30) {                                                             // struct
            uint32_t s = 0;
            for (size_t m = 2; m < d.size(); m++) s = std::max(s, member_offset[{t, uint32_t(m - 2)}] + self(self, d[m]));
            return s;
        }
        return 4;
    };
    std::vector<Binding> out;
    push_size = 0;
    for (auto& v : vars) {
        auto& pt = type[v.ptr_type];  // OpTypePointer result sc pointee
        uint32_t pointee = pt[3];
        if (v.sc == 9) {  // PushConstant
            push_size = size_of(size_of, pointee);
            continue;
        }
        if (!dbind.count(v.id)) continue;
        Binding b;
        b.set = dset[v.id];
        b.binding = dbind[v.id];
        uint32_t t = pointee;
        if ((type[t][0] & 0xFFFF) == 28) {  // arrayed binding
            b.count = constant[type[t][3]];
            t = type[t][2];
        } else if ((type[t][0] & 0xFFFF) == 29) {
            b.count = 1;
            t = type[t][2];
        }
        uint32_t top = type[t][0] & 0xFFFF;
        if (v.sc == 2) b.type = buffer_block[t] ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        else if (v.sc == 12) b.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        else if (top == 27) b.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        else if (top == 26) b.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        else if (top == 25) {  // OpTypeImage: dim, depth, arrayed, ms, sampled
            uint32_t dim = type[t][3], sampled = type[t][8];
            if (dim == 5) b.type = sampled == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            else b.type = sampled == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }
        if (b.type != VK_DESCRIPTOR_TYPE_MAX_ENUM) out.push_back(b);
    }
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: radv_stats a.spv [b.spv ...]\n");
        return 1;
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VkInstance inst;
    CHECK(vkCreateInstance(&ici, nullptr, &inst));
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(inst, &n, nullptr);
    if (!n) {
        fprintf(stderr, "no Vulkan device (RADV needs the amdgpu drm-shim: LD_PRELOAD + AMDGPU_GPU_ID=vangogh)\n");
        return 1;
    }
    std::vector<VkPhysicalDevice> pds(n);
    vkEnumeratePhysicalDevices(inst, &n, pds.data());
    VkPhysicalDevice pd = pds[0];
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pd, &props);
    fprintf(stderr, "device: %s (driver %u.%u.%u)\n", props.deviceName, VK_VERSION_MAJOR(props.driverVersion),
            VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion));

    float prio = 1;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pef{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR};
    pef.pipelineExecutableInfo = VK_TRUE;
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.pNext = &pef;
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &f12;
    vkGetPhysicalDeviceFeatures2(pd, &f2);
    pef.pipelineExecutableInfo = VK_TRUE;
    const char* exts[] = {VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME};
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &f2;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = exts;
    VkDevice dev;
    CHECK(vkCreateDevice(pd, &dci, nullptr, &dev));
    auto getProps = (PFN_vkGetPipelineExecutablePropertiesKHR)vkGetDeviceProcAddr(dev, "vkGetPipelineExecutablePropertiesKHR");
    auto getStats = (PFN_vkGetPipelineExecutableStatisticsKHR)vkGetDeviceProcAddr(dev, "vkGetPipelineExecutableStatisticsKHR");

    for (int a = 1; a < argc; a++) {
        auto spv = read_spv(argv[a]);
        uint32_t push = 0;
        auto binds = reflect(spv, push);
        std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> sets;
        for (auto& b : binds) sets[b.set].push_back({b.binding, b.type, b.count, VK_SHADER_STAGE_ALL, nullptr});
        uint32_t nsets = sets.empty() ? 0 : sets.rbegin()->first + 1;
        std::vector<VkDescriptorSetLayout> layouts(nsets);
        for (uint32_t s = 0; s < nsets; s++) {
            VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            auto& v = sets[s];
            li.bindingCount = uint32_t(v.size());
            li.pBindings = v.data();
            CHECK(vkCreateDescriptorSetLayout(dev, &li, nullptr, &layouts[s]));
        }
        VkPushConstantRange pr{VK_SHADER_STAGE_ALL, 0, (push + 3) & ~3u};
        VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pli.setLayoutCount = nsets;
        pli.pSetLayouts = layouts.data();
        pli.pushConstantRangeCount = push ? 1 : 0;
        pli.pPushConstantRanges = &pr;
        VkPipelineLayout pl;
        CHECK(vkCreatePipelineLayout(dev, &pli, nullptr, &pl));
        VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        smi.codeSize = spv.size() * 4;
        smi.pCode = spv.data();
        VkShaderModule sm;
        CHECK(vkCreateShaderModule(dev, &smi, nullptr, &sm));
        VkComputePipelineCreateInfo cpi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        cpi.flags = VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR;
        cpi.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, sm, "main"};
        cpi.layout = pl;
        VkPipeline pipe;
        auto t0 = std::chrono::steady_clock::now();
        CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpi, nullptr, &pipe));
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        printf("%s: compile %.0f ms\n", argv[a], ms);
        VkPipelineInfoKHR pinfo{VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};
        pinfo.pipeline = pipe;
        uint32_t ne = 0;
        getProps(dev, &pinfo, &ne, nullptr);
        std::vector<VkPipelineExecutablePropertiesKHR> ep(ne, {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR});
        getProps(dev, &pinfo, &ne, ep.data());
        for (uint32_t e = 0; e < ne; e++) {
            VkPipelineExecutableInfoKHR einfo{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR};
            einfo.pipeline = pipe;
            einfo.executableIndex = e;
            uint32_t ns = 0;
            getStats(dev, &einfo, &ns, nullptr);
            std::vector<VkPipelineExecutableStatisticKHR> st(ns, {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR});
            getStats(dev, &einfo, &ns, st.data());
            printf("  %s (subgroup %u):", ep[e].name, ep[e].subgroupSize);
            for (auto& s : st) {
                printf(" %s=", s.name);
                switch (s.format) {
                case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR: printf("%u", s.value.b32); break;
                case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR: printf("%lld", (long long)s.value.i64); break;
                case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR: printf("%llu", (unsigned long long)s.value.u64); break;
                case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR: printf("%.2f", s.value.f64); break;
                default: break;
                }
                printf(";");
            }
            printf("\n");
        }
        vkDestroyPipeline(dev, pipe, nullptr);
        vkDestroyShaderModule(dev, sm, nullptr);
        vkDestroyPipelineLayout(dev, pl, nullptr);
        for (auto l : layouts) vkDestroyDescriptorSetLayout(dev, l, nullptr);
    }
    vkDestroyDevice(dev, nullptr);
    vkDestroyInstance(inst, nullptr);
    return 0;
}
