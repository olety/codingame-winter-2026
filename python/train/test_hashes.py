from __future__ import annotations

import unittest
from pathlib import Path

from python.train.java_smoke import artifact_hash, behavior_hash


REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURE = REPO_ROOT / "rust/bot/configs/test_search_regression_v1.json"


class HashFixtureTests(unittest.TestCase):
    def test_shared_hash_fixture_matches_expected_constants(self) -> None:
        self.assertEqual(artifact_hash(FIXTURE), "3de772bb0ad664f0")
        self.assertEqual(behavior_hash(FIXTURE), "da326fa964e7bea0")


if __name__ == "__main__":
    unittest.main()
