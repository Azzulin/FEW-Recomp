// memcmp and bcmp for the Linux build.
//
// The Zig toolchain links its compiler-rt into the executable, and compiler-rt brings weak memcmp and bcmp that win
// over glibc's (a definition in the executable beats one in a shared library). Both compare one byte per iteration.
// In a dense battle on the Steam Deck that loop was ~35% of the GPU command thread (HWDER_SAMPLE_US): the renderer
// compares megabytes per frame (geometry and shader-memory mirrors, cache keys). These strong definitions replace
// them: 32 bytes per step with AVX2, which every supported CPU has (cpu_check.cpp).
//
// Built with -fno-builtin (CMakeLists.txt) so the compiler never turns a loop here into a call to memcmp or bcmp.
// Hidden: only this executable uses them; shared libraries loaded into the process keep glibc's.
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

namespace {
typedef uint64_t __attribute__((may_alias, aligned(1))) u64u;
typedef uint32_t __attribute__((may_alias, aligned(1))) u32u;
typedef uint16_t __attribute__((may_alias, aligned(1))) u16u;

inline __m256i load256(const uint8_t* p) { return _mm256_loadu_si256((const __m256i*)p); }
inline __m128i load128(const uint8_t* p) { return _mm_loadu_si128((const __m128i*)p); }
inline int order(const uint8_t* a, const uint8_t* b, size_t i) { return (int)a[i] - (int)b[i]; }
}  // namespace

extern "C" {

__attribute__((visibility("hidden"))) int bcmp(const void* pa, const void* pb, size_t n) noexcept {
    const uint8_t* a = (const uint8_t*)pa;
    const uint8_t* b = (const uint8_t*)pb;
    if (n >= 32) {
        size_t i = 0;
        for (; i + 128 <= n; i += 128) {
            const __m256i x0 = _mm256_xor_si256(load256(a + i), load256(b + i));
            const __m256i x1 = _mm256_xor_si256(load256(a + i + 32), load256(b + i + 32));
            const __m256i x2 = _mm256_xor_si256(load256(a + i + 64), load256(b + i + 64));
            const __m256i x3 = _mm256_xor_si256(load256(a + i + 96), load256(b + i + 96));
            const __m256i x = _mm256_or_si256(_mm256_or_si256(x0, x1), _mm256_or_si256(x2, x3));
            if (!_mm256_testz_si256(x, x)) return 1;
        }
        for (; i + 32 <= n; i += 32) {
            const __m256i x = _mm256_xor_si256(load256(a + i), load256(b + i));
            if (!_mm256_testz_si256(x, x)) return 1;
        }
        if (i == n) return 0;
        const __m256i x = _mm256_xor_si256(load256(a + n - 32), load256(b + n - 32));  // overlaps the last step
        return !_mm256_testz_si256(x, x);
    }
    if (n >= 16) {
        const __m128i x = _mm_or_si128(_mm_xor_si128(load128(a), load128(b)),
                                       _mm_xor_si128(load128(a + n - 16), load128(b + n - 16)));
        return !_mm_testz_si128(x, x);
    }
    if (n >= 8) return ((*(const u64u*)a ^ *(const u64u*)b) | (*(const u64u*)(a + n - 8) ^ *(const u64u*)(b + n - 8))) != 0;
    if (n >= 4) return ((*(const u32u*)a ^ *(const u32u*)b) | (*(const u32u*)(a + n - 4) ^ *(const u32u*)(b + n - 4))) != 0;
    if (n >= 2) return ((*(const u16u*)a ^ *(const u16u*)b) | (*(const u16u*)(a + n - 2) ^ *(const u16u*)(b + n - 2))) != 0;
    return n ? a[0] != b[0] : 0;
}

__attribute__((visibility("hidden"))) int memcmp(const void* pa, const void* pb, size_t n) noexcept {
    const uint8_t* a = (const uint8_t*)pa;
    const uint8_t* b = (const uint8_t*)pb;
    if (n >= 32) {
        size_t i = 0;
        for (; i + 32 <= n; i += 32) {
            const uint32_t differ = ~(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(load256(a + i), load256(b + i)));
            if (differ) return order(a, b, i + (size_t)__builtin_ctz(differ));
        }
        if (i == n) return 0;
        // The last step overlaps bytes already found equal, so its first difference is the first one overall.
        const uint32_t differ = ~(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(load256(a + n - 32), load256(b + n - 32)));
        return differ ? order(a, b, n - 32 + (size_t)__builtin_ctz(differ)) : 0;
    }
    if (n >= 16) {
        uint32_t differ = 0xFFFFu & ~(uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(load128(a), load128(b)));
        if (differ) return order(a, b, (size_t)__builtin_ctz(differ));
        differ = 0xFFFFu & ~(uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(load128(a + n - 16), load128(b + n - 16)));
        return differ ? order(a, b, n - 16 + (size_t)__builtin_ctz(differ)) : 0;
    }
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return order(a, b, i);
    return 0;
}

}  // extern "C"
