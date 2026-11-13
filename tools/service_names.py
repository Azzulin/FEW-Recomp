#!/usr/bin/env python3
"""Which Horizon service names appear (NUL-terminated) in an ExeFS, and which the runtime registers.

    python tools/service_names.py <exefs_dir> [<runtime src dir>]
"""
import os
import re
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nso  # noqa: E402

KNOWN = """sm: fsp-srv fsp-pr fsp-ldr hid hid:sys hid:dbg hidbus irs appletOE appletAE apm apm:p vi:u vi:s vi:m nvdrv
nvdrv:a nvdrv:s nvdrv:t nvmemp nvgem:c nvgem:cd dispdrv audren:u audout:u audin:u audrec:u auddev hwopus codecctl set
set:sys set:cal time:u time:s time:a acc:u0 acc:u1 pctl pctl:a pctl:s pctl:r psm nifm:u nifm:s nifm:a ns:am ns:am2 ns:ec
ns:su aoc:u mm:u lm nfp:user nfp:sys nfc:user nfc:am friend:u friend:a bsd:u bsd:s sfdnsres nsd:u nsd:a ssl csrng spl:
pl:u prepo:u prepo:a ldn:u ldn:s caps:a caps:c caps:u caps:ss caps:su pm:info fatal:u erpt:c btm gpio fgm fgm:0 fgm:9
pcv bpc lbl ectx:aw mii:u mii:e xcd:sys pdm:ntfy pdm:qry bcat:u bcat:a nim:shp ncm npns:u arp:r wlan:inf ovln:snd
grc:c avm aocsrv:u""".split()

proc = nso.Process(sys.argv[1])
found = {}
for m in proc.modules:
    img = bytes(m.image)
    for k in KNOWN:
        if b"\0" + k.encode() + b"\0" in img:
            found.setdefault(k, []).append(m.name)
registered = set()
if len(sys.argv) > 2:
    for dp, _, fs in os.walk(sys.argv[2]):
        for f in fs:
            if f.endswith(".cpp"):
                t = open(os.path.join(dp, f), encoding="utf-8", errors="replace").read()
                registered |= set(re.findall(r'(?:register_service|register_as<\w+>)\(\{?"([^"]+)"', t))
                registered |= set(re.findall(r'register_as<\w+>\(\{([^}]*)\}', t) and
                                  re.findall(r'"([a-z][\w:\-]*)"', " ".join(re.findall(r'register_as<\w+>\(\{([^}]*)\}', t))))
                registered |= set(re.findall(r'Service\("([a-z][\w:\-]*)"\)', t))
for k in sorted(found):
    mark = "" if not registered else ("  (runtime: sim)" if k in registered else "  (runtime: NAO)")
    print(f"{k:<10} {','.join(found[k])}{mark}")
