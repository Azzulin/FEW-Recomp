#!/bin/bash
# Teste de abertura no Steam Deck, pelo PC (Git Bash): roda o few por alguns segundos com a janela oculta e sem
# som (HWDER_OFFSCREEN + HWDER_MUTE: não aparece na tela de quem está no Deck), depois resume o log.
# Não roda se houver um jogo aberto no Deck. O log completo fica em <pasta>/hwder.log (o jogo o recria a cada
# execução; jogar depois sobrescreve).
#
# Uso:  bash tools/deck/testar-no-deck.sh [segundos=90] [pasta no Deck]
# Variáveis: DECK_HOST (padrão deck@<ip-do-deck>), DECK_KEY (padrão ~/.ssh/<chave-do-deck>),
#            DECK_DISPLAY (padrão :1, o Xwayland dos jogos no Modo Jogo; no Modo Desktop use :0)
set -euo pipefail
HOST=${DECK_HOST:-deck@<ip-do-deck>}
KEY=${DECK_KEY:-$HOME/.ssh/<chave-do-deck>}
SECS=${1:-90}
ssh_deck() { ssh -i "$KEY" -o BatchMode=yes -o ConnectTimeout=10 -o ServerAliveInterval=30 "$HOST" "$@"; }
DEST=${2:-$(ssh_deck -n 'for d in /run/media/deck/*/; do [ -d "$d/Games/FireEmblemWarriors" ] && { echo "${d%/}/Games/FireEmblemWarriors"; break; }; done')}
[ -n "$DEST" ] || { echo "ERRO: pasta FireEmblemWarriors não encontrada no Deck"; exit 1; }

ssh_deck bash -s -- "$DEST" "$SECS" "${DECK_DISPLAY:-:1}" <<'EOF'
set -u
DEST=$1 SECS=$2 DISP=$3
cd "$DEST" || exit 1
if pgrep -af 'reaper.*SteamLaunch|SteamLaunch' >/dev/null || pgrep -x few >/dev/null; then
    echo "ERRO: há um jogo aberto no Deck; teste cancelado"; exit 2
fi
[ -x few ] || chmod +x few
echo "== $(sha256sum few | cut -c1-16)…  few, teste de $SECS s em DISPLAY=$DISP"
export DISPLAY=$DISP XDG_RUNTIME_DIR=/run/user/$(id -u) HWDER_OFFSCREEN=1 HWDER_MUTE=1 HWDER_NO_PRELAUNCH=1
start=$(date +%s)
timeout -s INT "$SECS" ./few >/tmp/few-stdout.txt 2>&1
code=$?
pkill -x few 2>/dev/null
echo "== saiu com $code após $(( $(date +%s) - start )) s (124 = encerrado pelo teste)"
echo "== stdout/stderr (fim)"; tail -n 8 /tmp/few-stdout.txt
echo "== log"
grep -E 'vulkan: |nsp: (program|update|version)|aoc:|vi: first frame|display: 300 frames|fatal|error|unimplemented instruction|\[shader\]|mem: no write watch|write watch' hwder.log | head -40
echo "== últimas linhas"; tail -n 6 hwder.log
EOF
