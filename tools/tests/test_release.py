"""tools/release_notes.py: the release notes come from the version's CHANGELOG.md section, and a missing section stops
the release. python -m unittest tools/tests/test_release.py"""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import release_notes  # noqa: E402

CHANGELOG = """# Changelog

## Unreleased

- Something not released yet.

## FEW Recomp v0.5.0 (2026-11-28)

- **Setup:** pick your files and build.
- Another line.

## FEW Recomp v0.4.0 (2026-11-27)

- Older.
"""


class ReleaseNotesTest(unittest.TestCase):
    def test_takes_only_its_own_section(self):
        text = release_notes.notes("v0.5.0", CHANGELOG)
        self.assertIn("**Setup:** pick your files and build.", text)
        self.assertNotIn("Older.", text)
        self.assertNotIn("not released yet", text)
        self.assertIn("FEW-Recomp-v0.5.0-Windows-x64.zip", text)
        self.assertIn("blob/v0.5.0/CHANGELOG.md", text)

    def test_a_missing_section_stops_the_release(self):
        with self.assertRaises(SystemExit):
            release_notes.notes("v0.6.0", CHANGELOG)

    def test_a_version_prefix_is_not_a_match(self):
        self.assertIsNone(release_notes.section(CHANGELOG, "v0.5"))  # not v0.5.0's section

    def test_the_real_changelog_has_the_latest_released_version(self):
        text = (release_notes.ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
        self.assertIsNotNone(release_notes.section(text, "v0.4.0"))


if __name__ == "__main__":
    unittest.main()
