import copy
import unittest

from support import BOARDS, make_ctx, default_config
from config.boards import board_defines, button_count
from config.core import ConfigError
from generate_config import render


class SectionTests(unittest.TestCase):
    def setUp(self):
        self.data = default_config()

    def rejected(self, section, key, value, fragment=None):
        data = copy.deepcopy(self.data)
        data[section][key] = value
        with self.assertRaises(ConfigError) as caught:
            render(data, make_ctx())
        self.assertIn(f"{section}.{key}", str(caught.exception))
        if fragment:
            self.assertIn(fragment, str(caught.exception))

    def test_default_renders(self):
        header, features = render(self.data, make_ctx())
        self.assertIn("#pragma once", header)
        self.assertIn("namespace AppConfig {", header)
        self.assertIn('PROJECT_NAME = "Desk Gadget"', header)
        self.assertIn('WIFI_HOSTNAME = "desk-gadget"', header)
        self.assertIn('DEV_WIFI_SSID = ""', header)
        self.assertEqual(features["FEATURE_WIFI"], True)
        self.assertNotIn("FEATURE_BLE_HID", features)
        self.assertNotIn("FEATURE_DISPLAY", features)
        self.assertEqual(features["FEATURE_OTA"], False)  # no secrets

    def test_optional_sections_default(self):
        for key in ("wifi", "ota", "gestures"):
            del self.data[key]
        header, features = render(self.data, make_ctx())
        self.assertIn("LONG_PRESS_MS = 650;", header)
        self.assertEqual(features["FEATURE_WIFI"], False)

    def test_bounds(self):
        self.rejected("project", "name", "")
        self.rejected("project", "name", "x" * 33)
        self.rejected("wifi", "hostname", "-bad")
        self.rejected("wifi", "hostname", "a" * 33)
        self.rejected("wifi", "hostname", "has_underscore")
        self.rejected("gestures", "debounce_ms", 0)
        self.rejected("gestures", "double_click_ms", 49)
        self.rejected("gestures", "long_press_ms", 99)

    def test_gesture_ordering_message_unchanged(self):
        self.data["gestures"] = {"debounce_ms": 100, "double_click_ms": 80, "long_press_ms": 650}
        with self.assertRaises(ConfigError) as caught:
            render(self.data, make_ctx())
        self.assertIn("Gesture timing must satisfy debounce_ms < double_click_ms < long_press_ms",
                      str(caught.exception))

    def test_removed_sections_are_unknown_keys(self):
        for key in ("ble", "display", "buttons"):
            data = copy.deepcopy(self.data)
            data[key] = {}
            with self.assertRaises(ConfigError) as caught:
                render(data, make_ctx())
            self.assertIn(key, str(caught.exception))

    def test_disabled_features(self):
        self.data["wifi"]["enabled"] = False
        _, features = render(self.data, make_ctx(secrets={"ota_password": "pw"}))
        self.assertFalse(features["FEATURE_WIFI"])
        self.assertFalse(features["FEATURE_OTA"])  # OTA needs WiFi

    def test_ota_with_password(self):
        ctx = make_ctx(secrets={"ota_password": 'p"w'})
        header, features = render(self.data, ctx)
        self.assertTrue(features["FEATURE_OTA"])
        self.assertIn("#if FEATURE_OTA", header)
        self.assertIn('OTA_PASSWORD = "p\\"w"', header)

    def test_ota_without_password(self):
        ctx = make_ctx(secrets={})
        header, features = render(self.data, ctx)
        self.assertFalse(features["FEATURE_OTA"])
        self.assertNotIn("OTA_PASSWORD", header)
        self.assertIn("OTA disabled: no ota_password in secrets.yaml", ctx.warnings)

    def test_ota_forced_off_without_wifi_emits_no_password(self):
        self.data["wifi"]["enabled"] = False
        ctx = make_ctx(secrets={"ota_password": "pw"})
        header, features = render(self.data, ctx)
        self.assertFalse(features["FEATURE_OTA"])
        self.assertNotIn("OTA_PASSWORD", header)
        self.assertIn("OTA disabled: wifi.enabled is false", ctx.warnings)

    def test_dev_wifi_from_secrets(self):
        header, _ = render(self.data, make_ctx(secrets={"wifi_ssid": "net", "wifi_password": "pw"}))
        self.assertIn('DEV_WIFI_SSID = "net"', header)
        self.assertIn('DEV_WIFI_PASSWORD = "pw"', header)


class BoardTests(unittest.TestCase):
    def defines(self, board):
        _, plain = board_defines("test", board, {}, "v0")
        return dict(plain)

    def test_heltec_row_emits_epd_pins(self):
        defines = self.defines(BOARDS["wireless-paper"])
        self.assertEqual(defines["BOARD_DRIVER_HELTEC"], 1)
        self.assertEqual(defines["BOARD_EPD_MOSI"], 2)
        self.assertEqual(defines["BOARD_EPD_SCK"], 3)
        self.assertEqual(defines["BOARD_EPD_CS"], 4)
        self.assertEqual(defines["BOARD_EPD_DC"], 5)
        self.assertEqual(defines["BOARD_EPD_RST"], 6)
        self.assertEqual(defines["BOARD_EPD_BUSY"], 7)
        self.assertEqual(defines["BOARD_POWER_PIN"], 45)
        self.assertEqual(defines["BOARD_POWER_ACTIVE_LOW"], 1)
        self.assertEqual(defines["BOARD_EPD_PANEL_FC1"], 1)
        self.assertEqual(defines["BOARD_BUTTON_COUNT"], 1)
        self.assertEqual(defines["BOARD_HAS_DISPLAY"], 1)
        self.assertNotIn("BOARD_EPD_PANEL_BN", defines)

    def test_every_panel_gets_one_macro(self):
        for panel in ("bn", "fc1", "e0213a367"):
            board = {**BOARDS["wireless-paper"], "epaper": {**BOARDS["wireless-paper"]["epaper"], "panel": panel}}
            self.assertEqual(self.defines(board)[f"BOARD_EPD_PANEL_{panel.upper()}"], 1)

    def test_unknown_driver_is_error(self):
        with self.assertRaises(ConfigError) as caught:
            board_defines("x", {**BOARDS["wireless-paper"], "driver": "m5"}, {}, "v0")
        self.assertIn("driver", str(caught.exception))

    def test_unknown_panel_is_error(self):
        board = {**BOARDS["wireless-paper"], "epaper": {**BOARDS["wireless-paper"]["epaper"], "panel": "zz"}}
        with self.assertRaises(ConfigError) as caught:
            board_defines("x", board, {}, "v0")
        self.assertIn("epaper.panel", str(caught.exception))

    def test_button_count_from_pins_or_explicit(self):
        self.assertEqual(button_count({"button_pins": [0]}), 1)
        self.assertEqual(button_count({"buttons": 2}), 2)
        self.assertEqual(button_count({}), 0)


if __name__ == "__main__":
    unittest.main()
