"""Rewrite a Windows HWDER_SAMPLE_US report with function names (the exe's DWARF is not readable by DbgHelp).

    python tools/symbolize_samples.py build/run/hwder.log.samples.txt [few.exe]

Every "?0x<address>" (an address inside the executable) is resolved with llvm-symbolizer from the HWDER toolchain
(or PATH) and the counts of rows that become the same function are merged. Writes <report>.sym.txt.
"""
import glob
import os
import re
import shutil
import subprocess
import sys
from collections import OrderedDict


def symbolizer():
    tools = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Packages")
    hits = glob.glob(os.path.join(tools, "Claude_*", "LocalCache", "Local", "HWDER", "managed-tools", "*", "output", "llvm",
                                  "*", "bin", "llvm-symbolizer.exe"))
    return hits[0] if hits else shutil.which("llvm-symbolizer")


def main():
    report = sys.argv[1]
    exe = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(report), "few.exe")
    text = open(report, encoding="utf-8", errors="replace").read()
    addrs = sorted(set(re.findall(r"\?(0x[0-9a-f]+)", text)))
    out = subprocess.run([symbolizer(), "--obj=" + exe, "--functions=short", "--no-inlines", "--output-style=GNU"],
                         input="\n".join(addrs) + "\n", capture_output=True, text=True).stdout.splitlines()
    names = {}
    for k, a in enumerate(addrs):  # GNU style without inlines: one function line and one file:line per address
        fn = out[2 * k] if 2 * k < len(out) else "?"
        names[a] = fn if fn != "??" else "?" + a
    text = re.sub(r"\?(0x[0-9a-f]+)", lambda m: names.get(m.group(1), m.group(0)), text)
    # merge rows "count pct name" that became identical names within each section
    lines, merged = text.splitlines(), []
    section = OrderedDict()
    def flush():
        for name, c in sorted(section.items(), key=lambda kv: -kv[1][0]):
            merged.append(f"{c[0]:8d} {c[1]:5.1f}% {name}")
        section.clear()
    for l in lines:
        m = re.match(r"\s*(\d+)\s+([\d.]+)%\s+(.*)", l)
        if m:
            k = m.group(3)
            c = section.get(k, [0, 0.0])
            section[k] = [c[0] + int(m.group(1)), c[1] + float(m.group(2))]
        else:
            flush()
            merged.append(l)
    flush()
    open(report + ".sym.txt", "w", encoding="utf-8").write("\n".join(merged) + "\n")
    print(report + ".sym.txt")


if __name__ == "__main__":
    main()
