import configparser
import unittest

from support import ROOT, BOARDS
from list_envs import list_envs
from package_firmware import default_envs


class DefaultEnvsTests(unittest.TestCase):
    def test_default_envs_match_list_envs(self):
        parser = configparser.ConfigParser(interpolation=None, strict=False)
        parser.read(ROOT / "platformio.ini", encoding="utf-8")
        raw = parser.get("platformio", "default_envs", fallback="")
        defaults = raw.replace(",", " ").split()
        self.assertEqual(sorted(defaults), sorted(list_envs(ROOT / "platformio.ini")))
        self.assertNotIn("native", defaults)

    def test_default_envs_exclude_aliases_and_native(self):
        options = {
            "wireless-paper": {},
            "wireless-paper-alt": {"custom_board": "wireless-paper"},
            "native": {},
            "unknown-board": {},
        }
        self.assertEqual(default_envs(options, BOARDS), ["wireless-paper"])


if __name__ == "__main__":
    unittest.main()
