#!/usr/bin/env bash
# Steam Deck shader bench without a Deck: translates compute programs dumped by HWDER_DUMP_SHADERS with the
# current translator and compiles them with Mesa's RADV for the Deck's GPU (amdgpu noop drm-shim, AMDGPU_GPU_ID=
# vangogh), printing the compiler's statistics (VGPRs, spills, scratch, instructions, compile time).
#
# The shim is not packaged by Arch. Build it once from the Mesa source matching the installed vulkan-radeon:
#   cd /root/mesa && wget https://archive.mesa3d.org/mesa-<ver>.tar.xz && tar xf mesa-<ver>.tar.xz && cd mesa-<ver>
#   meson setup b -Dvulkan-drivers=amd -Dgallium-drivers= -Dtools=drm-shim -Dplatforms= -Dglx=disabled -Degl=disabled
#       -Dgles1=disabled -Dgles2=disabled -Dopengl=false -Dllvm=disabled -Dvideo-codecs= -Dbuildtype=release
#   ninja -C b src/amd/drm-shim/libamdgpu_noop_drm_shim.so
# and point DRM_SHIM at it (default below).
#
#   wsl -d archlinux -u root -- bash "/mnt/c/.../tools/shadertest/wsl_bench.sh" [key ...]
#
# Needs in the distro: clang, vulkan-headers, vulkan-icd-loader, vulkan-radeon. Keys default to the three big
# particle compute shaders of the battle. Listings come from build/run/shader_<key>_6.maxwell.txt and the
# workgroup/shared size from the .cbufs.txt next to them. Output in /root/st.
set -euo pipefail
R="$(cd "$(dirname "$0")/../.." && pwd)"
W=/root/st
mkdir -p "$W"
cd "$W"
if [ ! -x radv_stats ] || [ "$R/tools/shadertest/radv_stats.cpp" -nt radv_stats ]; then
    clang++ -std=c++20 -O1 "$R/tools/shadertest/radv_stats.cpp" -lvulkan -o radv_stats
fi
clang++ -std=c++20 -O2 -w -DNOMINMAX -I"$R/runtime/include" -I"$R/runtime/src" \
    -I"$R/third_party/spirv-headers/include" "$R/tools/shadertest/shadertest.cpp" "$R"/runtime/src/shader/*.cpp \
    "$R/runtime/src/debug_flags.cpp" -o shadertest
keys=("$@")
[ ${#keys[@]} -eq 0 ] && keys=(a49eaa020c32da6e abbede64c0dec758 13be8c9400da3059)
spvs=()
for k in "${keys[@]}"; do
    lst="$R/build/run/shader_${k}_6.maxwell.txt"
    cb="$R/build/run/shader_${k}_6.cbufs.txt"
    python3 "$R/tools/shadertest/listing_to_bin.py" "$lst" "$W/$k.cs.bin" >/dev/null
    read -r lx ly lz sm < <(sed -n 's/^grid .*, local \([0-9]*\) \([0-9]*\) \([0-9]*\), smem \([0-9]*\).*/\1 \2 \3 \4/p' "$cb")
    ./shadertest one "$W/$k.cs.bin" "$W/$k.spv" "${lx:-64}" "${ly:-1}" "${lz:-1}" "${sm:-0}" 2>"$W/$k.log" >/dev/null
    if command -v spirv-val >/dev/null; then spirv-val "$W/$k.spv" || echo "$k: spirv-val FAILED"; fi
    spvs+=("$W/$k.spv")
done
SHIM="${DRM_SHIM:-$(ls /root/mesa/mesa-*/b/src/amd/drm-shim/libamdgpu_noop_drm_shim.so 2>/dev/null | head -1)}"
if [ -z "$SHIM" ] || [ ! -f "$SHIM" ]; then
    echo "SPIR-V in $W; no amdgpu drm-shim for the RADV statistics (see the header of this script)"
    exit 0
fi
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/radeon_icd.json LD_PRELOAD="$SHIM" AMDGPU_GPU_ID=vangogh ./radv_stats "${spvs[@]}"
