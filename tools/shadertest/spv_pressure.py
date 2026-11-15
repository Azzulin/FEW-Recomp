"""Register pressure estimate of a SPIR-V program: the most SSA values alive at once.

    spirv-opt --ssa-rewrite --eliminate-dead-code-aggressive a.spv -o a.ssa.spv
    spirv-dis a.ssa.spv | python3 tools/shadertest/spv_pressure.py

A stand-in for the driver's own statistics when no driver is at hand (the Steam Deck's compiler, ACO, needs an AMD
device or Mesa's drm-shim). Counts every value defined in the function body (no types, constants or globals),
32-bit or not, so it is an upper-bound style figure for comparing two translations of the same program, not a VGPR
count. Prints the maximum, the mean over instructions and the number of values live into loop headers.
"""
import re
import sys

ID = re.compile(r"%[\w.]+")


def main():
    lines = sys.stdin.read().splitlines()
    blocks, order, cur = {}, [], None
    in_fn = False
    loop_headers = set()
    for raw in lines:
        s = raw.split(";")[0].strip()
        if not s:
            continue
        if "OpFunction " in s or s.endswith("OpFunction") or re.search(r"= OpFunction\b", s):
            in_fn = True
            continue
        if s.startswith("OpFunctionEnd"):
            in_fn = False
            cur = None
            continue
        if not in_fn:
            continue
        if "OpLabel" in s:
            cur = s.split("=")[0].strip()
            blocks[cur] = {"insts": [], "succ": [], "phis": []}
            order.append(cur)
            continue
        if cur is None:
            continue
        res = None
        body = s
        if "=" in s.split("Op")[0]:
            res, body = [x.strip() for x in s.split("=", 1)]
        parts = body.split()
        op = parts[0]
        ids = ID.findall(body)
        if op == "OpPhi":
            # %r = OpPhi %type %v1 %b1 %v2 %b2 ...
            pairs = ids[1:]
            blocks[cur]["phis"].append((res, [(pairs[i], pairs[i + 1]) for i in range(0, len(pairs) - 1, 2)]))
            continue
        if op == "OpLoopMerge":
            loop_headers.add(cur)
            continue
        if op == "OpSelectionMerge":
            continue
        if op == "OpBranch":
            blocks[cur]["succ"] = [ids[0]]
            continue
        if op == "OpBranchConditional":
            blocks[cur]["succ"] = ids[1:3]
            blocks[cur]["insts"].append((None, [ids[0]]))
            continue
        if op == "OpSwitch":
            blocks[cur]["succ"] = [x for x in ids[1:]]
            blocks[cur]["insts"].append((None, [ids[0]]))
            continue
        if op == "OpVariable":
            continue
        # result type is the first id after the opcode for value-producing instructions
        uses = ids[1:] if res else ids
        blocks[cur]["insts"].append((res, uses))

    defined = set()
    for b in blocks.values():
        for r, _ in b["phis"]:
            defined.add(r)
        for r, _ in b["insts"]:
            if r:
                defined.add(r)

    def vals(xs):
        return {x for x in xs if x in defined}

    # per-block use/def
    info = {}
    for name, b in blocks.items():
        use, de = set(), set(r for r, _ in b["phis"])
        for r, us in b["insts"]:
            for u in vals(us):
                if u not in de:
                    use.add(u)
            if r:
                de.add(r)
        info[name] = (use, de)
    live_in = {n: set() for n in blocks}
    live_out = {n: set() for n in blocks}
    changed = True
    while changed:
        changed = False
        for n in reversed(order):
            b = blocks[n]
            out = set()
            for s in b["succ"]:
                if s not in blocks:
                    continue
                sb = blocks[s]
                phi_defs = {r for r, _ in sb["phis"]}
                out |= live_in[s] - phi_defs
                for _, inc in sb["phis"]:
                    for v, p in inc:
                        if p == n and v in defined:
                            out.add(v)
            use, de = info[n]
            inn = use | (out - de) | {r for r, _ in b["phis"]}
            if out != live_out[n] or inn != live_in[n]:
                live_out[n], live_in[n] = out, inn
                changed = True
    peak, total, count = 0, 0, 0
    for n in order:
        live = set(live_out[n])
        for r, us in reversed(blocks[n]["insts"]):
            if r:
                live.discard(r)
            live |= vals(us)
            peak = max(peak, len(live))
            total += len(live)
            count += 1
    hdr = max((len(live_in[h]) for h in loop_headers), default=0)
    print(f"peak {peak} mean {total / max(count, 1):.1f} loop-header-max {hdr} blocks {len(order)} loops {len(loop_headers)}")


if __name__ == "__main__":
    main()
