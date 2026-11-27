"""tools/build_events.py: the events file the build window reads, and the compile progress taken from ninja.
No compiler, game or network: run with `python -m unittest tools/tests/test_build_events.py`."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_events  # noqa: E402


class Clock:
    def __init__(self, t=0.0):
        self.t = t

    def __call__(self):
        return self.t


class EventLogTest(unittest.TestCase):
    def test_start_truncates_and_lists_the_steps(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "sub" / "events.jsonl"
            log = build_events.EventLog(path, clock=Clock(100.0))
            log.emit("step", id="old")
            log.start("Fire Emblem Warriors", log="/tmp/build.log")
            log.emit("step", id="compile")
            events = [json.loads(l) for l in path.read_text(encoding="utf-8").splitlines()]
            self.assertEqual([e["type"] for e in events], ["start", "step"])
            self.assertEqual([s["id"] for s in events[0]["steps"]], [i for i, _ in build_events.STEPS])
            self.assertEqual(events[0]["t"], 100.0)

    def test_emit_without_the_variable_writes_nothing(self):
        old = os.environ.pop("FEW_EVENTS", None)
        try:
            build_events.emit("step", id="check")  # must not raise or write anywhere
            self.assertIsNone(build_events.EventLog.from_env())
        finally:
            if old is not None:
                os.environ["FEW_EVENTS"] = old

    def test_non_ascii_text_stays_readable(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "e.jsonl"
            build_events.EventLog(path).emit("error", text="Arquivo não encontrado: 'Fire Emblem Musou'")
            self.assertIn("não", path.read_text(encoding="utf-8"))


class PlanTest(unittest.TestCase):
    def test_weights_follow_the_source_size_and_the_link_gets_its_share(self):
        with tempfile.TemporaryDirectory() as root:
            gen = Path(root) / "generated"
            gen.mkdir()
            (gen / "rc_main_000.c").write_bytes(b"x" * 10_000_000)
            (gen / "rc_main_001.c").write_bytes(b"x" * 10)
            lines = ["ninja: Entering directory `build/pc'",
                     "[1/3] Building C object CMakeFiles/hwder_game.dir/generated/rc_main_000.c.obj",
                     "[2/3] Building C object CMakeFiles/hwder_game.dir/generated/rc_main_001.c.obj",
                     "[3/3] Linking CXX executable few.exe"]
            weights = build_events.plan(lines, root)
            big = weights["Building C object CMakeFiles/hwder_game.dir/generated/rc_main_000.c.obj"]
            small = weights["Building C object CMakeFiles/hwder_game.dir/generated/rc_main_001.c.obj"]
            self.assertEqual(big, 10_000_000)
            self.assertEqual(small, build_events.MIN_WEIGHT)
            self.assertAlmostEqual(weights["Linking CXX executable few.exe"], (big + small) * build_events.LINK_SHARE)

    def test_one_huge_function_weighs_more_than_many_small_ones(self):
        huge = b"void f_1(Ctx* c) {\n" + b"  x = 1;\n" * 250_000 + b"}\n"
        many = b"".join(b"void f_%d(Ctx* c) {\n" % i + b"  x = 1;\n" * 250 + b"}\n" for i in range(1000))
        self.assertAlmostEqual(len(huge), len(many), delta=len(huge) * 0.05)
        self.assertGreater(build_events.cost(huge), 2 * build_events.cost(many))
        self.assertEqual(build_events.cost(huge.replace(b"\n", b"\r\n")) > build_events.cost(many), True)

    def test_linux_objects_end_in_o(self):
        with tempfile.TemporaryDirectory() as root:
            (Path(root) / "runtime").mkdir()
            (Path(root) / "runtime" / "core.cpp").write_bytes(b"x" * 100_000)
            self.assertEqual(build_events.weigh("Building CXX object CMakeFiles/hwder.dir/runtime/core.cpp.o", root),
                             100_000)


class CompileProgressTest(unittest.TestCase):
    def test_weighted_fraction_time_left_and_finished_files(self):
        clock = Clock()
        progress = build_events.CompileProgress({"Building C object a.c.obj": 900.0, "Building C object b.c.obj": 100.0},
                                                clock=clock)
        self.assertIsNone(progress.line("ninja: Entering directory `build'"))
        clock.t = 30.0
        event = progress.line("[1/2] Building C object b.c.obj\n")
        self.assertEqual((event["done"], event["total"], event["fraction"]), (1, 2, 0.1))
        self.assertEqual(event["files"], ["b.c"])
        self.assertEqual(event["eta"], 270)  # 30 s for 10% of the work
        clock.t = 300.0
        event = progress.line("[2/2] Building C object a.c.obj")
        self.assertEqual(event["fraction"], 1.0)
        self.assertIsNone(event["eta"])

    def test_no_time_left_too_early(self):
        clock = Clock()
        progress = build_events.CompileProgress({"Building C object a.c.obj": 10.0, "b": 990.0}, clock=clock)
        clock.t = 5.0
        self.assertIsNone(progress.line("[1/2] Building C object a.c.obj")["eta"])  # 1% done, 5 s in

    def test_without_a_plan_the_steps_are_counted(self):
        progress = build_events.CompileProgress({})
        self.assertEqual(progress.line("[3/4] Linking CXX executable few")["fraction"], 0.75)


if __name__ == "__main__":
    unittest.main()
