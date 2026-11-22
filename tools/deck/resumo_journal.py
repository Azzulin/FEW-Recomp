"""Resumo por janela de uma sessão jogada no Deck, a partir do journal (ou de um hwder.log com horário).

Junta, a cada linha "display: 300 frames", o fps, o pior quadro, o tempo de present/limiter e os quadros não mostrados
com as estatísticas da thread da GPU do mesmo trecho (ocupação, envios, esperas, despachos e draws), e intercala os
eventos que mudam o ritmo (F2, modo de exibição, intervalo de troca, resolução 3D, início/fim).

Pegar o journal no Deck (a Steam joga a saída do jogo nele; o hwder.log é sobrescrito a cada execução):
    ssh -i ~/.ssh/<chave-do-deck> deck@<ip> 'journalctl --user -S "AAAA-MM-DD HH:MM" -o short-iso | grep -F "[hwder]"' > journal.txt
Uso: python tools/deck/resumo_journal.py journal.txt
"""
import re
import sys

STAMP = re.compile(r"^\d{4}-\d\d-\d\dT(\d\d:\d\d:\d\d)\S* .*?\[hwder\] (.*)")


def main(path):
    cur = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = STAMP.match(line)
            if not m:
                continue
            t, s = m.groups()
            if r := re.match(r"display: 300 frames: avg ([\d.]+) ms, worst ([\d.]+).*present ([\d.]+) ms, limiter ([\d.]+) ms"
                             r"(?: \| (\d+) not shown)?", s):
                d = cur
                print(f"{t} fps {1000 / float(r[1]):5.1f} pior {float(r[2]):6.1f} present {float(r[3]):5.1f} "
                      f"limiter {float(r[4]):5.1f} nao mostrados {r[5] or '-':>3} | gpuT {d.get('busy', -1):5.1f} ms "
                      f"envios {d.get('subm', -1):4.1f} espera fence {d.get('fw', -1):5.2f} | despachos/q "
                      f"{d.get('dpf', -1):6.1f} draws {d.get('draws', -1)}")
                cur = {}
            elif r := re.match(r"vk: gpu thread busy ([\d.]+) ms per frame.*?; ([\d.]+) submits", s):
                cur["busy"], cur["subm"] = float(r[1]), float(r[2])
            elif r := re.match(r"gpu: thread waits.*fence ([\d.]+) ms", s):
                cur["fw"] = float(r[1])
            elif r := re.match(r"vk: frame \d+: draws (\d+)", s):
                cur["draws"] = int(r[1])
            elif r := re.match(r"gpu: per frame: ([\d.]+) dispatches", s):
                cur["dpf"] = float(r[1])
            elif (s.startswith(("few: ", "display: swap interval", "display: the host", "HWDER "))
                  or "window closed" in s or "stall:" in s):
                print(f"{t}   ## {s[:160]}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
