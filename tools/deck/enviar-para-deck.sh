#!/bin/bash
# Envia ".\Envio Deck\FireEmblemWarriors" ao Steam Deck por SSH (Git Bash no Windows): só o que mudou, retoma
# arquivo cortado e confere o SHA-256 de cada arquivo nos dois lados. Adaptado do enviar-para-deck.sh do HWDER.
#
# Uso:  bash tools/deck/enviar-para-deck.sh [/run/media/deck/<uuid-do-SD>/Games/FireEmblemWarriors]
#       sem argumento: a pasta Games do primeiro cartão SD montado no Deck
# Variáveis: DECK_HOST (padrão deck@<ip-do-deck>), DECK_KEY (padrão ~/.ssh/<chave-do-deck>)
#            ENVIAR_INI=1  sobrescreve também o hwder_settings.ini do Deck (por padrão o de lá é mantido, com os
#                          ajustes feitos no F1)
# O Deck precisa estar ACORDADO e com o sshd ativo (no Deck: sudo systemctl enable --now sshd).
set -euo pipefail
HOST=${DECK_HOST:-deck@<ip-do-deck>}
KEY=${DECK_KEY:-$HOME/.ssh/<chave-do-deck>}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SRC="$ROOT/Envio Deck/FireEmblemWarriors"
[ -f "$SRC/few" ] || { echo "ERRO: monte a pasta antes (.\\gerar-deck.ps1)"; exit 1; }

ssh_deck() { ssh -i "$KEY" -o BatchMode=yes -o ConnectTimeout=10 -o ServerAliveInterval=30 "$HOST" "$@"; }
q() { printf "'%s'" "${1//\'/\'\\\'\'}"; }   # aspas para o shell remoto

if [ $# -ge 1 ]; then
    DEST=$1
else
    SD=$(ssh_deck -n 'for d in /run/media/deck/*/; do [ -d "$d/Games" ] && { echo "${d%/}"; break; }; done')
    [ -n "$SD" ] || { echo "ERRO: nenhum cartão com pasta Games em /run/media/deck"; exit 1; }
    DEST="$SD/Games/FireEmblemWarriors"
fi
echo "destino: $HOST:$DEST"

ssh_deck -n "pgrep -x few >/dev/null && { echo 'ERRO: feche o jogo no Deck'; exit 3; }; mkdir -p $(q "$DEST")/save"

cd "$SRC"
mapfile -t files < <(find . -type f ! -path './save/*' ! -name '*.bak' | sed 's|^\./||' | sort)
for f in "${files[@]}"; do
    rf="$DEST/$f"
    ssh_deck -n "mkdir -p $(q "$(dirname "$rf")")"
    lsize=$(stat -c %s "$f")
    rsize=$(ssh_deck -n "stat -c %s $(q "$rf") 2>/dev/null || echo 0")
    if [ "$f" = hwder_settings.ini ] && [ "$rsize" != 0 ] && [ "${ENVIAR_INI:-}" != 1 ]; then
        echo "manter   $f (já existe no Deck; ENVIAR_INI=1 para sobrescrever)"; continue
    fi
    if [ "$rsize" = "$lsize" ]; then
        action="conferir"
    elif [ "$rsize" -gt 0 ] && [ "$rsize" -lt "$lsize" ] &&
         [ "$(head -c "$rsize" "$f" | sha256sum | cut -d' ' -f1)" = "$(ssh_deck -n "sha256sum $(q "$rf") | cut -d' ' -f1")" ]; then
        action="retomar"
        echo "retomar  $f: faltam $(( (lsize - rsize) / 1048576 )) MiB"
        tail -c +$((rsize + 1)) "$f" | ssh_deck "cat >> $(q "$rf")"
    else
        action="enviar"
        echo "enviar   $f ($(( lsize / 1048576 )) MiB)"
        ssh_deck "cat > $(q "$rf.part") && mv -f $(q "$rf.part") $(q "$rf")" < "$f"
    fi
    l=$(sha256sum "$f" | cut -d' ' -f1)
    r=$(ssh_deck -n "sha256sum $(q "$rf") | cut -d' ' -f1")
    if [ "$l" = "$r" ]; then echo "ok       $f [$action]"
    else echo "ERRO: $f difere após $action (local $l, Deck $r)"; exit 1; fi
done

ssh_deck -n "cd $(q "$DEST") && chmod +x few few-steam.sh instalar-no-deck.sh"
echo
echo "Pronto em $DEST. Para criar o atalho na Steam (uma vez), no Deck:"
echo "    cd $(q "$DEST") && sh instalar-no-deck.sh"
