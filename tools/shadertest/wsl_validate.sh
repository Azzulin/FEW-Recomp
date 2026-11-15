#!/usr/bin/env bash
# Translates every program dumped by HWDER_DUMP_SHADERS (build/run/shader_<key>.<stage>.bin) with the current
# translator and validates the SPIR-V with spirv-val (Vulkan 1.1 rules, what the renderer targets). Run in WSL:
#
#   wsl -d archlinux -u root -- bash "/mnt/c/.../tools/shadertest/wsl_validate.sh" [dir]
#
# Needs clang and spirv-tools in the distro. Prints one line per failing program and a summary; also counts the
# programs that fell back to the block dispatcher ("structured control flow failed" in the translator log).
set -uo pipefail
R="$(cd "$(dirname "$0")/../.." && pwd)"
DIR="${1:-$R/build/run}"
W=/root/st-val
rm -rf "$W" && mkdir -p "$W/in" "$W/out"
clang++ -std=c++20 -O2 -w -DNOMINMAX -I"$R/runtime/include" -I"$R/runtime/src" -I"$R/third_party/spirv-headers/include" \
    "$R/tools/shadertest/shadertest.cpp" "$R"/runtime/src/shader/*.cpp "$R/runtime/src/debug_flags.cpp" -o "$W/shadertest" || exit 1
cp "$DIR"/shader_*.bin "$W/in/" 2>/dev/null
n=$(ls "$W/in" | wc -l)
echo "programs: $n"
"$W/shadertest" run "$W/in" "$W/out" 2> "$W/translate.log"
fails=0
for f in "$W"/out/*.spv; do
    if ! out=$(spirv-val --target-env vulkan1.1 "$f" 2>&1); then
        fails=$((fails + 1))
        echo "FAIL $(basename "$f"): $(echo "$out" | head -2 | tr '\n' ' ')"
    fi
done
echo "spirv-val failures: $fails / $n"
echo "dispatcher fallbacks: $(grep -c 'structured control flow failed' "$W/translate.log")"
grep 'structured control flow failed\|translation failed\|translation error' "$W/translate.log" | sort | uniq -c | head
