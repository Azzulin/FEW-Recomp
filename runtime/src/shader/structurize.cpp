// Structured control flow for multi-block programs.
//
// The block graph (translate.cpp build_cfg: SYNC/BRK/CONT/RET already resolved into jumps) is rebuilt as nested
// SPIR-V selections and loops instead of the loop+switch dispatcher. The dispatcher is valid everywhere but costly
// on AMD (Steam Deck): the switch becomes a chain of compares walked at every jump, and every guest register that
// crosses a block boundary is a loop-carried value live through the whole program, so large compute programs spill
// to scratch. Structured output gives the driver the program's real shape and short register lifetimes.
//
// The method follows Norman Ramsey's "Beyond Relooper: Recursive Translation of Unstructured Control Flow to
// Structured Control Flow" (ICFP 2022) for reducible graphs, built on the dominator tree:
//   - a node with two or more forward predecessors is a merge node; its code is placed after a Block (a construct
//     whose end the predecessors jump to) inside the code of its immediate dominator;
//   - a loop header's code is wrapped in a Loop construct; back edges continue it;
//   - any other successor has a single predecessor and its code is inlined at the branch.
// Two changes for GPUs, where lanes must reconverge where the original program reconverged (barriers, warp votes):
//   - a node outside a loop whose immediate dominator is inside it (the loop's exit targets and what they lead to) is
//     placed after the loop, not inside it (Ramsey inlines it in the loop body, which is fine for a CPU);
//   - Block and Loop are both SPIR-V loops (a Block runs once), and the innermost Block around a Loop shares its
//     merge, so a loop exit is a plain break.
// SPIR-V only allows breaking the innermost loop. A jump to an outer construct (a multi-level break or continue)
// stores a code in a function variable and breaks; each construct's merge forwards it outward until the target
// takes it. Irreducible graphs throw StructureFailed and the caller translates again with the dispatcher.
#include <algorithm>
#include <cstdio>
#include <functional>
#include <set>

#include "shader/translator.h"

namespace shader {

namespace {

class Structurizer {
public:
    Structurizer(Translator& t) : t(t), b(t.b) {}

    void run() {
        analyze();
        do_tree(0);
        if (b.block_open) throw StructureFailed("program end left open");
        if (!ctx.empty()) throw StructureFailed("unbalanced constructs");
#ifdef HW_SHADERTEST
        int nloops = int(loops.size()), nmerge = 0;
        for (char m : is_merge) nmerge += m;
        fprintf(stderr, "structure: %d blocks, %d loops, %d merge nodes, %d constructs, %d multi-level jumps\n", n,
                nloops, nmerge, stat_constructs, stat_multi);
#endif
    }

private:
    Translator& t;
    SpvBuilder& b;

    // ---- graph analysis
    int n = 0;
    std::vector<std::vector<int>> succ, preds;
    std::vector<int> rpo, idom, fwd_preds, place, loop_of_header, innermost;
    std::vector<char> is_merge;
    struct Loop {
        int header;
        int parent;  // enclosing loop (index) or -1
        std::vector<char> body;
    };
    std::vector<Loop> loops;
    std::vector<std::vector<int>> merge_children;

    bool dominates(int a, int x) const {
        for (;;) {
            if (x == a) return true;
            if (x == 0) return false;
            x = idom[x];
        }
    }

    void analyze() {
        n = int(t.blocks.size());
        succ.resize(n);
        preds.resize(n);
        for (int i = 0; i < n; i++) succ[i] = t.successors(t.blocks[i]);

        // reverse postorder from the entry (block 0)
        std::vector<int> post;
        std::vector<char> seen(n, 0);
        std::vector<std::pair<int, size_t>> stack{{0, 0}};
        seen[0] = 1;
        while (!stack.empty()) {
            auto& [x, i] = stack.back();
            if (i < succ[x].size()) {
                int y = succ[x][i++];
                if (!seen[y]) {
                    seen[y] = 1;
                    stack.push_back({y, 0});
                }
            } else {
                post.push_back(x);
                stack.pop_back();
            }
        }
        rpo.assign(n, -1);
        std::vector<int> order(post.rbegin(), post.rend());
        for (size_t i = 0; i < order.size(); i++) rpo[order[i]] = int(i);
        for (int x : order)
            for (int y : succ[x]) preds[y].push_back(x);

        // immediate dominators (Cooper, Harvey, Kennedy: "A Simple, Fast Dominance Algorithm")
        idom.assign(n, -1);
        idom[0] = 0;
        auto intersect = [&](int a, int c) {
            while (a != c) {
                while (rpo[a] > rpo[c]) a = idom[a];
                while (rpo[c] > rpo[a]) c = idom[c];
            }
            return a;
        };
        for (bool changed = true; changed;) {
            changed = false;
            for (size_t i = 1; i < order.size(); i++) {
                int x = order[i], nd = -1;
                for (int p : preds[x]) {
                    if (idom[p] < 0) continue;
                    nd = nd < 0 ? p : intersect(p, nd);
                }
                if (nd != idom[x]) {
                    idom[x] = nd;
                    changed = true;
                }
            }
        }

        // back edges (reducibility), forward predecessor counts
        fwd_preds.assign(n, 0);
        std::vector<std::vector<int>> back_srcs(n);
        for (int x : order)
            for (int y : succ[x]) {
                if (rpo[y] <= rpo[x]) {
                    if (!dominates(y, x)) throw StructureFailed("irreducible control flow");
                    back_srcs[y].push_back(x);
                } else {
                    fwd_preds[y]++;
                }
            }

        // natural loops, outer headers first (an inner header has a higher RPO number)
        loop_of_header.assign(n, -1);
        innermost.assign(n, -1);
        for (int h : order) {
            if (back_srcs[h].empty()) continue;
            Loop l{h, innermost[h], std::vector<char>(n, 0)};
            l.body[h] = 1;
            std::vector<int> work = back_srcs[h];
            while (!work.empty()) {
                int x = work.back();
                work.pop_back();
                if (l.body[x]) continue;
                l.body[x] = 1;
                for (int p : preds[x]) work.push_back(p);
            }
            int li = int(loops.size());
            loop_of_header[h] = li;
            for (int x = 0; x < n; x++)
                if (l.body[x]) innermost[x] = li;
            loops.push_back(std::move(l));
        }

        // placement: the immediate dominator, or after the outermost loop that holds the dominator but not the node
        place.assign(n, -1);
        is_merge.assign(n, 0);
        merge_children.assign(n, {});
        for (int v : order) {
            if (v == 0) continue;
            int p = idom[v], lifted = -1;
            for (int l = innermost[p]; l >= 0 && !loops[l].body[v]; l = loops[l].parent) lifted = l;
            place[v] = lifted >= 0 ? loops[lifted].header : p;
            is_merge[v] = fwd_preds[v] >= 2 || lifted >= 0;
            if (is_merge[v]) merge_children[place[v]].push_back(v);
        }
    }

    // ---- emission
    struct Ctx {
        int cont_node = -1;  // loop header whose back edges continue this construct
        int brk_node = -1;   // node whose code follows this construct (jumps to it break)
        Id merge = 0, cont = 0;
        std::set<u32> pending;  // multi-level jump codes arriving at the merge
    };
    std::vector<Ctx> ctx;
    Id brk_var = 0;
    int stat_constructs = 0, stat_multi = 0;

    static u32 code_brk(int node) { return u32(node) * 2 + 1; }
    static u32 code_cont(int node) { return u32(node) * 2 + 2; }

    void by_rpo_desc(std::vector<int>& v) {
        std::sort(v.begin(), v.end(), [&](int a, int c) { return rpo[a] > rpo[c]; });
    }

    void do_tree(int x) {
        std::vector<int> inner, outer;
        int li = loop_of_header[x];
        for (int v : merge_children[x]) (li >= 0 && !loops[li].body[v] ? outer : inner).push_back(v);
        by_rpo_desc(inner);
        by_rpo_desc(outer);
        auto node = [&] { blocks_around(inner, 0, [&] { emit_node(x); }, -1); };
        if (li < 0) node();
        else blocks_around(outer, 0, node, x);
    }

    // Block(ys[i]) { Block(ys[i+1]) { ... body ... } ys[i+1] code } ys[i] code, with ys sorted by decreasing RPO
    // number; loop_header >= 0 puts body in Loop(loop_header), fused with the innermost Block.
    void blocks_around(const std::vector<int>& ys, size_t i, const std::function<void()>& body, int loop_header) {
        if (i == ys.size()) {
            if (loop_header >= 0) {
                Id pending = construct(loop_header, -1, body);
                close(pending);
            } else {
                body();
            }
            return;
        }
        int y = ys[i];
        Id pending;
        if (i + 1 == ys.size() && loop_header >= 0) pending = construct(loop_header, y, body);
        else pending = construct(-1, y, [&] { blocks_around(ys, i + 1, body, loop_header); });
        if (b.block_open) do_tree(y);
        close(pending);
    }

    void close(Id pending) {
        if (b.block_open) throw StructureFailed("construct follow left open");
        if (pending) {
            b.label(pending);
            b.terminate(spv::OpUnreachable);
        }
    }

    // A SPIR-V loop: continued by back edges to cont_node, left by jumps to brk_node, whose code the caller emits
    // next. Returns with the block open when the follow code runs (and, if multi-level jumps pass through, inside
    // a selection whose unreachable merge the caller closes: the returned label), or closed when nothing follows.
    Id construct(int cont_node, int brk_node, const std::function<void()>& body) {
        Id header = b.id(), body_l = b.id(), cont = b.id(), merge = b.id();
        stat_constructs++;
        b.label(header);
        b.op0(spv::OpLoopMerge, {merge, cont, 0});
        b.branch(body_l);
        b.label(body_l);
        ctx.push_back({cont_node, brk_node, merge, cont, {}});
        body();
        if (b.block_open) b.branch(merge);  // falling off the end leaves the construct
        Ctx c = std::move(ctx.back());
        ctx.pop_back();
        b.label(cont);
        b.branch(header);
        b.label(merge);
        if (c.pending.empty()) {
            if (brk_node < 0) b.terminate(spv::OpUnreachable);
            return 0;
        }
        if (brk_node < 0) {  // only multi-level jumps reach this merge
            forward(c.pending);
            return 0;
        }
        Id v = b.load(t.t_u32, brk_var);
        Id pend = b.op(spv::OpINotEqual, t.t_bool, {v, b.c_uint(0)});
        Id fwd = b.id(), normal = b.id(), sel_merge = b.id();
        b.op0(spv::OpSelectionMerge, {sel_merge, 0});
        b.terminate(spv::OpBranchConditional, {pend, fwd, normal});
        b.label(fwd);
        forward(c.pending);
        b.label(normal);
        return sel_merge;
    }

    // Multi-level jumps that left a construct: the enclosing one takes those aimed at it and passes the rest on.
    void forward(const std::set<u32>& codes) {
        if (ctx.empty()) throw StructureFailed("multi-level jump with no target");
        Ctx& p = ctx.back();
        Id v = b.load(t.t_u32, brk_var);
        bool rest = false;
        for (u32 code : codes) {
            Id target = 0;
            if (p.brk_node >= 0 && code == code_brk(p.brk_node)) target = p.merge;
            else if (p.cont_node >= 0 && code == code_cont(p.cont_node)) target = p.cont;
            if (!target) {
                rest = true;
                p.pending.insert(code);
                continue;
            }
            Id yes = b.id(), no = b.id();
            Id eq = b.op(spv::OpIEqual, t.t_bool, {v, b.c_uint(code)});
            b.op0(spv::OpSelectionMerge, {no, 0});
            b.terminate(spv::OpBranchConditional, {eq, yes, no});
            b.label(yes);
            b.store(brk_var, b.c_uint(0));
            b.branch(target);
            b.label(no);
        }
        if (rest) b.branch(p.merge);
        else b.terminate(spv::OpUnreachable);
    }

    // Jump to the construct that continues `node` (cont) or is followed by `node`.
    void go(int node, bool cont) {
        if (ctx.empty()) throw StructureFailed("jump with no enclosing construct");
        Ctx& c = ctx.back();
        if (cont ? c.cont_node == node : c.brk_node == node) {
            b.branch(cont ? c.cont : c.merge);
            return;
        }
        bool found = false;
        for (auto& e : ctx) found |= cont ? e.cont_node == node : e.brk_node == node;
        if (!found) throw StructureFailed("jump target is not an enclosing construct");
        if (!brk_var) brk_var = b.local_var(t.ptr_fn_u32, b.c_uint(0));
        u32 code = cont ? code_cont(node) : code_brk(node);
        stat_multi++;
        b.store(brk_var, b.c_uint(code));
        c.pending.insert(code);
        b.branch(c.merge);
    }

    // Ramsey's doBranch: continue, break, or the successor's code inline.
    void jump(int from, int to) {
        if (rpo[to] <= rpo[from]) go(to, true);
        else if (is_merge[to]) go(to, false);
        else do_tree(to);
        if (b.block_open) throw StructureFailed("branch left open");
    }

    bool is_go(int from, int to) const { return rpo[to] <= rpo[from] || is_merge[to]; }

    void emit_node(int x) {
        Translator::Term tm = t.emit_block_body(t.blocks[x]);
        if (tm.kind == 0) {
            if (b.block_open) throw StructureFailed("terminated block left open");
            return;
        }
        if (tm.kind == 1 || tm.t == tm.f) {
            jump(x, tm.t);
            return;
        }
        bool gt = is_go(x, tm.t), gf = is_go(x, tm.f);
        if (gt != gf) {
            // One side leaves (break/continue), the other is inlined: `if (c) leave;` then the code after the
            // selection, so chains of early exits do not nest.
            int leave = gt ? tm.t : tm.f, stay = gt ? tm.f : tm.t;
            Id cond = gt ? tm.cond : b.op(spv::OpLogicalNot, t.t_bool, {tm.cond});
            Id yes = b.id(), m = b.id();
            b.op0(spv::OpSelectionMerge, {m, 0});
            b.terminate(spv::OpBranchConditional, {cond, yes, m});
            b.label(yes);
            jump(x, leave);
            b.label(m);
            jump(x, stay);
            return;
        }
        Id tl = b.id(), fl = b.id(), m = b.id();
        b.op0(spv::OpSelectionMerge, {m, 0});
        b.terminate(spv::OpBranchConditional, {tm.cond, tl, fl});
        b.label(tl);
        jump(x, tm.t);
        b.label(fl);
        jump(x, tm.f);
        b.label(m);
        b.terminate(spv::OpUnreachable);
    }
};

}  // namespace

void Translator::emit_structured() {
    Structurizer s(*this);
    s.run();
}

}  // namespace shader
