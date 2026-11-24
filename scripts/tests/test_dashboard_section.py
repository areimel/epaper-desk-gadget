"""Tests for the dashboard config section. It is imported directly and, for the
render tests, registered temporarily, so these pass before and after the
orchestrator adds it to sections/__init__.py."""
import copy
import unittest
from unittest import mock

from support import ROOT, make_ctx, default_config
from config import sections
from config.core import ConfigError
from config.sections import dashboard
from generate_config import render

NAMES = ("MESSAGES_MAX", "MESSAGE_TEXT_MAX", "MESSAGE_BY_MAX", "DWELL_MIN_S", "DWELL_MAX_S")


def lim():
    return dashboard.read_limits()


def validate(value):
    return dashboard.validate(value, make_ctx())


class LimitsParserTests(unittest.TestCase):
    def test_finds_every_name_in_the_real_header(self):
        limits = lim()
        for name in NAMES:
            self.assertIn(name, limits)
            self.assertGreater(limits[name], 0)

    def test_matches_header_text(self):
        text = (ROOT / "lib/feed/src/Limits.h").read_text(encoding="utf-8")
        self.assertIn(f"DWELL_MIN_S = {lim()['DWELL_MIN_S']};", text)

    def test_missing_name_fails_loudly(self):
        with self.assertRaises(ConfigError) as caught:
            dashboard.parse_limits("constexpr uint8_t MESSAGES_MAX = 40;")
        self.assertIn("MESSAGE_TEXT_MAX", str(caught.exception))


class ValidateTests(unittest.TestCase):
    def rejected(self, value, path):
        with self.assertRaises(ConfigError) as caught:
            validate(value)
        self.assertIn(path, str(caught.exception))

    def test_defaults(self):
        cfg = validate(dashboard.DEFAULT)
        self.assertTrue(cfg["enabled"])
        self.assertEqual(cfg["dwell_s"], lim()["DWELL_DEFAULT_S"])
        self.assertEqual(cfg["messages"], [])

    def test_repo_config_validates(self):
        cfg = validate(default_config()["dashboard"])
        self.assertGreaterEqual(len(cfg["messages"]), 25)

    def test_dwell_bounds(self):
        low, high = lim()["DWELL_MIN_S"], lim()["DWELL_MAX_S"]
        self.assertEqual(validate({"dwell_s": low})["dwell_s"], low)
        self.assertEqual(validate({"dwell_s": high})["dwell_s"], high)
        self.rejected({"dwell_s": low - 1}, "dashboard.dwell_s")
        self.rejected({"dwell_s": high + 1}, "dashboard.dwell_s")
        self.rejected({"dwell_s": "300"}, "dashboard.dwell_s")

    def test_enabled_must_be_bool(self):
        self.rejected({"enabled": "yes"}, "dashboard.enabled")

    def test_text_limit_is_bytes(self):
        limit = lim()["MESSAGE_TEXT_MAX"]
        validate({"messages": [{"text": "a" * limit}]})
        self.rejected({"messages": [{"text": "a" * (limit + 1)}]}, "dashboard.messages[0].text")
        # Two-byte characters: half the limit in characters fits, one more does not.
        validate({"messages": [{"text": "é" * (limit // 2)}]})
        self.rejected({"messages": [{"text": "é" * (limit // 2 + 1)}]}, "dashboard.messages[0].text")

    def test_by_limit_is_bytes(self):
        limit = lim()["MESSAGE_BY_MAX"]
        validate({"messages": [{"text": "x", "by": "b" * limit}]})
        self.rejected({"messages": [{"text": "x"}, {"text": "x", "by": "b" * (limit + 1)}]},
                      "dashboard.messages[1].by")

    def test_count_limit(self):
        most = lim()["MESSAGES_MAX"]
        validate({"messages": [{"text": "x"}] * most})
        self.rejected({"messages": [{"text": "x"}] * (most + 1)}, "dashboard.messages")

    def test_shape_errors(self):
        self.rejected({"messages": "hello"}, "dashboard.messages")
        self.rejected({"messages": ["hello"]}, "dashboard.messages[0]")
        self.rejected({"messages": [{"by": "me"}]}, "dashboard.messages[0]")
        self.rejected({"messages": [{"text": ""}]}, "dashboard.messages[0].text")
        self.rejected({"messages": [{"text": 5}]}, "dashboard.messages[0].text")
        self.rejected({"messages": [{"text": "x", "by": 5}]}, "dashboard.messages[0].by")
        self.rejected({"messages": [{"text": "a\nb"}]}, "dashboard.messages[0].text")
        self.rejected("on", "dashboard")

    def test_unknown_keys(self):
        self.rejected({"dwell": 300}, "dashboard.dwell")
        self.rejected({"messages": [{"text": "x", "author": "y"}]}, "dashboard.messages[0].author")

    def test_by_is_optional(self):
        cfg = validate({"messages": [{"text": "Hi"}]})
        self.assertEqual(cfg["messages"], [{"text": "Hi", "by": ""}])


class EmitTests(unittest.TestCase):
    def emit(self, cfg):
        return "\n".join(dashboard.emit(validate(cfg), make_ctx()))

    def test_lines(self):
        out = self.emit({"dwell_s": 600, "messages": [{"text": 'Say "hi"?', "by": "Me"}, {"text": "Two"}]})
        self.assertIn("constexpr uint16_t DEFAULT_DWELL_S = 600;", out)
        self.assertIn("struct StarterMessage { const char* text; const char* by; };", out)
        self.assertIn("constexpr StarterMessage STARTER_MESSAGES[] = {", out)
        self.assertIn('{"Say \\"hi\\"\\?", "Me"},', out)
        self.assertIn('{"Two", ""},', out)
        self.assertIn("constexpr uint8_t STARTER_MESSAGE_COUNT = 2;", out)

    def test_empty_list_still_compiles(self):
        out = self.emit({})
        self.assertIn("constexpr uint8_t STARTER_MESSAGE_COUNT = 0;", out)
        self.assertIn('{"", ""}', out)  # one dummy element: zero-size arrays are not C++


class FeatureTests(unittest.TestCase):
    def render(self, wifi, dash=True):
        data = copy.deepcopy(default_config())
        data["wifi"]["enabled"] = wifi
        data["dashboard"]["enabled"] = dash
        modules = [m for m in sections.BUILTIN if m is not dashboard] + [dashboard]
        ctx = make_ctx()
        with mock.patch.object(sections, "BUILTIN", modules):
            header, features = render(data, ctx)
        return header, features, ctx

    def test_on_with_wifi(self):
        header, features, _ = self.render(wifi=True)
        self.assertTrue(features["FEATURE_DASHBOARD"])
        self.assertIn("STARTER_MESSAGES", header)

    def test_off_without_wifi(self):
        _, features, ctx = self.render(wifi=False)
        self.assertFalse(features["FEATURE_DASHBOARD"])
        self.assertIn("dashboard disabled: wifi.enabled is false", ctx.warnings)

    def test_off_when_disabled(self):
        _, features, ctx = self.render(wifi=True, dash=False)
        self.assertFalse(features["FEATURE_DASHBOARD"])
        self.assertNotIn("dashboard disabled: wifi.enabled is false", ctx.warnings)


if __name__ == "__main__":
    unittest.main()
