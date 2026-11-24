#!/bin/bash
# Compila o few para Linux / Steam Deck dentro do WSL (archlinux), a partir desta árvore e do C já gerado
# no Windows (generated/, saída do gerar-few.ps1). Chamado por ..\..\gerar-deck.ps1.
#
# Toolchain: o fixado pelo setup Linux do HWDER v0.3.5 (Zig/Clang mirando glibc 2.35, CMake, Ninja), em
# /root/hwder/data/HWDER/toolchain/<id> se já instalado à mão; senão o few_build.py --tools-only instala em
# $WORK/tools. Dependências: linux-deps/ da release Linux oficial do HWDER (SDL3, FFmpeg, libcrypto estáticos).
# O binário só precisa de libc/libm e roda no SteamOS 3.
#
# CPU: HW_MARCH=znver2 (Zen 2, a APU do Deck): código agendado e com as instruções dele. Não roda em CPU sem elas.
#
# Uso: bash tools/deck/build-deck-wsl.sh /mnt/c/.../repo
# Variáveis: FEW_WSL_WORK (padrão /root/few), HWDER_WSL_WORK (padrão /root/hwder), FEW_JOBS (padrão 10),
#            FEW_MARCH (padrão znver2; vazio = AVX2 genérico)
set -euo pipefail

SRC_WIN=${1:?uso: build-deck-wsl.sh /mnt/c/.../repo}
WORK=${FEW_WSL_WORK:-/root/few}
HW=${HWDER_WSL_WORK:-/root/hwder}
JOBS=${FEW_JOBS:-10}
MARCH=${FEW_MARCH-znver2}
DEPS=$(ls -d "$HW"/release/HWDER-linux-x86_64-*/linux-deps 2>/dev/null | head -1)
TC=$(ls -d "$HW"/data/HWDER/toolchain/*/ 2>/dev/null | head -1)
if [ -z "$DEPS" ] || [ -z "$TC" ] || [ ! -x "$TC/bin/zig-cc" ]; then
    # Sem a instalação manual em $HW: o mesmo instalador do build-linux.sh baixa e confere tudo (SHA-256) em
    # $WORK/tools, uma vez.
    command -v python3 >/dev/null || { echo "ERRO: falta python3 no WSL (Ubuntu: apt install python3)"; exit 1; }
    tools_out=$(python3 "$SRC_WIN/tools/linux/few_build.py" --tools-only --tools-dir "$WORK/tools" | tee /dev/stderr)
    TC=$(printf '%s\n' "$tools_out" | sed -n 's/^TOOLCHAIN=//p'); DEPS=$(printf '%s\n' "$tools_out" | sed -n 's/^LINUX_DEPS=//p')
fi
[ -n "$DEPS" ] || { echo "ERRO: falta linux-deps"; exit 1; }
[ -n "$TC" ] && [ -x "$TC/bin/zig-cc" ] || { echo "ERRO: falta o toolchain Linux"; exit 1; }
[ -f "$SRC_WIN/generated/sources.cmake" ] || { echo "ERRO: falta generated/ (rode gerar-few.ps1 antes)"; exit 1; }

# Cópia para o ext4 do WSL (compilar lendo de /mnt/c é várias vezes mais lento). Só o necessário ao build.
mkdir -p "$WORK/src"
for item in CMakeLists.txt runtime third_party tools generated LICENSE; do
    [ -e "$SRC_WIN/$item" ] && cp -ru --preserve=timestamps "$SRC_WIN/$item" "$WORK/src/"
done
# Fontes apagadas no Windows não podem continuar compilando aqui: o CMake lista os .c do generated/.
echo "== fontes em $WORK/src ($(du -sh "$WORK/src" | cut -f1))"

export PATH="$TC/bin:$TC/cmake/bin:$(dirname "$TC/ninja/ninja"):$TC/python/bin:/usr/bin:/bin"
export CC="$TC/bin/zig-cc" CXX="$TC/bin/zig-c++" HWDER_AR="$TC/bin/zig-ar" HWDER_RANLIB="$TC/bin/zig-ranlib"
export HWDER_LINUX_DEPS="$DEPS" ZIG_GLOBAL_CACHE_DIR="$TC/zig-cache" ZIG_LOCAL_CACHE_DIR="$TC/zig-cache"
export PYTHONNOUSERSITE=1 PYTHONUTF8=1
unset CFLAGS CXXFLAGS LDFLAGS LD_PRELOAD LD_LIBRARY_PATH

cd "$WORK/src"
date
python3 tools/build.py --jobs "$JOBS" --build-dir "$WORK/build-deck" --generated-dir "$WORK/src/generated" \
    --define "HW_MARCH=$MARCH" --define HW_GAME_HOOKS=OFF
date
OUT="$WORK/build-deck/few"
ls -la "$OUT"
echo "== bibliotecas exigidas (só libc/libm/ld) e glibc máxima (<= 2.35):"
readelf -d "$OUT" | grep NEEDED || true
readelf -V "$OUT" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1
sha256sum "$OUT"
