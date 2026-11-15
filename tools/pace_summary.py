"""Frame pacing summary from a hwder.log recorded with HWDER_PACE_TRACE=1.

Each presented frame logs "[  s.ms] pace: present dt <ms> iv <swap interval> ...". The report splits the run into
windows (default 20 s) and prints, per window: presents, fps, median and 95th percentile frame time, the share of
frames slower than the threshold (default 20 ms, i.e. below 50 fps) and the swap interval the game asked for
(1 = 60 Hz, 2 = 30 Hz). The first frames of a window that straddles a loading screen show up as a high p95.

Usage: python tools/pace_summary.py build/run/hwder.log [--window 20] [--slow-ms 20] [--from 100 --to 230]
"""
import argparse
import collections
import re
import statistics
import sys

LINE = re.compile(r"^\[\s*(\d+)\.(\d+)\]\s+pace: present dt ([\d.]+) iv (\d+)")


def read(path):
    rows = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.match(line)
            if m:
                rows.append((int(m[1]) + int(m[2]) / 1000.0, float(m[3]), int(m[4])))
    return rows


def pct(sorted_vals, p):
    if not sorted_vals:
        return 0.0
    i = min(len(sorted_vals) - 1, max(0, round(p / 100.0 * (len(sorted_vals) - 1))))
    return sorted_vals[i]


def line(label, rows, span, slow_ms):
    dts = sorted(r[1] for r in rows)
    ivs = collections.Counter(r[2] for r in rows)
    slow = sum(1 for d in dts if d > slow_ms)
    iv = "/".join(f"{k}:{v * 100 // len(rows)}%" for k, v in sorted(ivs.items()))
    return (f"{label:>13}  {len(rows):6d}  {len(rows) / span:6.1f}  {statistics.median(dts):7.2f}  {pct(dts, 95):7.2f}"
            f"  {pct(dts, 99):7.2f}  {slow * 100.0 / len(rows):6.1f}%  {iv}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log")
    ap.add_argument("--window", type=float, default=20.0, help="window length in seconds (default 20)")
    ap.add_argument("--slow-ms", type=float, default=20.0, help="frame time counted as slow (default 20 ms)")
    ap.add_argument("--from", dest="t0", type=float, default=None, help="start of the overall range (s)")
    ap.add_argument("--to", dest="t1", type=float, default=None, help="end of the overall range (s)")
    a = ap.parse_args()

    rows = read(a.log)
    if not rows:
        sys.exit(f"no 'pace: present' lines in {a.log} (run with HWDER_PACE_TRACE=1)")
    print(f"{'window (s)':>13}  {'frames':>6}  {'fps':>6}  {'median':>7}  {'p95':>7}  {'p99':>7}  "
          f"{'>' + format(a.slow_ms, 'g') + 'ms':>7}  interval")
    start = rows[0][0]
    buckets = collections.defaultdict(list)
    for r in rows:
        buckets[int((r[0] - start) // a.window)].append(r)
    for k in sorted(buckets):
        lo = start + k * a.window
        hi = min(lo + a.window, rows[-1][0])
        print(line(f"{lo:5.0f}-{hi:<5.0f}", buckets[k], max(hi - lo, 1e-3), a.slow_ms))
    t0 = a.t0 if a.t0 is not None else rows[0][0]
    t1 = a.t1 if a.t1 is not None else rows[-1][0]
    sel = [r for r in rows if t0 <= r[0] <= t1]
    if sel:
        print(line(f"{t0:5.0f}-{t1:<5.0f}", sel, max(t1 - t0, 1e-3), a.slow_ms) + "  (range)")


if __name__ == "__main__":
    main()
