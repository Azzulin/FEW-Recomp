#!/bin/bash
# Teste de abertura no Steam Deck, pelo PC (Git Bash): roda o few por alguns segundos com a janela oculta e sem
# som (HWDER_OFFSCREEN + HWDER_MUTE: não aparece na tela de quem está no Deck), depois resume o log.
# Não roda se houver um jogo aberto no Deck. O log completo fica em <pasta>/hwder.log (o jogo o recria a cada
# execução; jogar depois sobrescreve).
#
# Uso:  bash tools/deck/testar-no-deck.sh [segundos=90] [pasta no Deck]
# Variáveis: DECK_HOST (padrão deck@<ip-do-deck>), DECK_KEY (padrão ~/.ssh/<chave-do-deck>),
#            DECK_DISPLAY (padrão :1, o Xwayland dos jogos no Modo Jogo; no Modo Desktop use :0)
#            DECK_INPUT=tools/input/battle_long.txt  roteiro de entrada (HWDER_INPUT_SCRIPT), copiado para o Deck
#            DECK_FRESH=1  começa com save vazio: o save do Deck é guardado antes e volta no fim (o do teste some)
#            DECK_ENV="HWDER_THREAD_STATS=1 ..."  variáveis extras para o jogo
#            DECK_SERIAL=1  sem a sobreposição de quadros (HWDER_FRAME_OVERLAP), que o hwder_env.txt do Deck liga (com ele no Deck, renomeie-o para medir em série)
#            DECK_PACE=1  liga HWDER_PACE_TRACE, traz as linhas "pace:" para build/deck_pace.log e resume o fps por
#                         janela de 20 s com tools/pace_summary.py
#
# Batalha medida (save vazio até o prólogo e 5 min de luta, ~230 s):
#   DECK_FRESH=1 DECK_PACE=1 DECK_INPUT=tools/input/battle_long.txt bash tools/deck/testar-no-deck.sh 230
set -euo pipefail
HOST=${DECK_HOST:-deck@<ip-do-deck>}
KEY=${DECK_KEY:-$HOME/.ssh/<chave-do-deck>}
SECS=${1:-90}
ssh_deck() { ssh -i "$KEY" -o BatchMode=yes -o ConnectTimeout=10 -o ServerAliveInterval=30 "$HOST" "$@"; }
DEST=${2:-$(ssh_deck -n 'for d in /run/media/deck/*/; do [ -d "$d/Games/FireEmblemWarriors" ] && { echo "${d%/}/Games/FireEmblemWarriors"; break; }; done')}
[ -n "$DEST" ] || { echo "ERRO: pasta FireEmblemWarriors não encontrada no Deck"; exit 1; }

INPUT_REMOTE=""
if [ -n "${DECK_INPUT:-}" ]; then
    INPUT_REMOTE=/tmp/few-input.txt
    ssh_deck "cat > $INPUT_REMOTE" < "$DECK_INPUT"
fi

EXTRA_ENV=${DECK_ENV:-}
[ -n "${DECK_PACE:-}" ] && EXTRA_ENV="$EXTRA_ENV HWDER_PACE_TRACE=1"
# Como no few-steam.sh: sobreposição de quadros ligada; DECK_SERIAL=1 mede o modo em série.
[ -n "${DECK_SERIAL:-}" ] || EXTRA_ENV="$EXTRA_ENV HWDER_FRAME_OVERLAP=1"

# O ssh junta os argumentos numa linha que a shell do Deck separa de novo: cada um vai citado (%q), senão um
# DECK_ENV com espaços vira vários argumentos e só a primeira variável chega ao jogo.
ssh_deck "bash -s -- $(printf '%q ' "$DEST" "$SECS" "${DECK_DISPLAY:-:1}" "$INPUT_REMOTE" "${DECK_FRESH:-}" "$EXTRA_ENV")" <<'EOF'
set -u
DEST=$1 SECS=$2 DISP=$3 INPUT=$4 FRESH=$5 EXTRA=$6
cd "$DEST" || exit 1
if pgrep -af 'reaper.*SteamLaunch|SteamLaunch' >/dev/null || pgrep -x few >/dev/null; then
    echo "ERRO: há um jogo aberto no Deck; teste cancelado"; exit 2
fi
[ -x few ] || chmod +x few
# Como no few-steam.sh: as chaves que faltam num .ini de build antigo entram com o padrão do Deck (30 fps a 720p).
if [ -f hwder_settings.ini ]; then
    for kv in fps60=0 fps60_1080p=0 fps30_720p=1; do
        grep -q "^${kv%%=*}=" hwder_settings.ini && continue
        [ -n "$(tail -c 1 hwder_settings.ini)" ] && echo >> hwder_settings.ini  # o .ini pode terminar sem quebra de linha
        echo "$kv" >> hwder_settings.ini
    done
fi
echo "== $(sha256sum few | cut -c1-16)…  few, teste de $SECS s em DISPLAY=$DISP${INPUT:+, roteiro $INPUT}${FRESH:+, save vazio}"
export DISPLAY=$DISP XDG_RUNTIME_DIR=/run/user/$(id -u) HWDER_OFFSCREEN=1 HWDER_MUTE=1 HWDER_NO_PRELAUNCH=1
[ -n "$INPUT" ] && export HWDER_INPUT_SCRIPT=$INPUT
for kv in $EXTRA; do export "$kv"; done
kept=""
if [ -n "$FRESH" ] && [ -d save ]; then
    kept="save.antes-do-teste-$(date +%Y%m%d-%H%M%S)"
    mv save "$kept"
fi
start=$(date +%s)
timeout -s INT "$SECS" ./few >/tmp/few-stdout.txt 2>&1
code=$?
pkill -x few 2>/dev/null
if [ -n "$FRESH" ]; then  # the test's save goes; the one from before comes back
    rm -rf save
    if [ -n "$kept" ]; then mv "$kept" save; else mkdir -p save; fi
fi
echo "== saiu com $code após $(( $(date +%s) - start )) s (124 = encerrado pelo teste)"
echo "== stdout/stderr (fim)"; tail -n 8 /tmp/few-stdout.txt
echo "== log"
grep -E 'vulkan: |nsp: (program|update|version)|aoc:|vi: first frame|display: 300 frames|fatal|error|unimplemented instruction|\[shader\]|mem: no write watch|write watch|frame fence|hang: gpu|gpu thread busy|per frame: [0-9.]+ dispatches' hwder.log | head -80
echo "== últimas linhas"; tail -n 6 hwder.log
EOF

if [ -n "${DECK_PACE:-}" ]; then
    ROOT=$(cd "$(dirname "$0")/../.." && pwd)
    mkdir -p "$ROOT/build"
    ssh_deck -n "grep 'pace: present' '$DEST/hwder.log'" > "$ROOT/build/deck_pace.log" || true
    echo "== ritmo de quadros (build/deck_pace.log)"
    PY=""  # no Windows, "python3" pode ser o atalho da Microsoft Store, que não roda nada
    for c in python3 python; do "$c" -c '' 2>/dev/null && { PY=$c; break; }; done
    [ -n "$PY" ] && "$PY" "$ROOT/tools/pace_summary.py" "$ROOT/build/deck_pace.log" || true
fi
