"""Board table lookup and the compiler defines derived from it."""
import subprocess

from config.core import ConfigError, load_yaml


def load_boards(path):
    return load_yaml(path)


def select_board(boards, board_id):
    if board_id not in boards:
        raise ConfigError("custom_board", f"unknown board {board_id!r}; known: {sorted(boards)}")
    return boards[board_id]


def firmware_version(cwd):
    try:
        out = subprocess.run(["git", "describe", "--tags", "--always"], cwd=cwd, capture_output=True,
                             text=True, timeout=10, check=True).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        out = ""
    return out or "0.0.0-dev"


DRIVERS = ("heltec",)
PANELS = ("bn", "fc1", "e0213a367")
EPD_PINS = ("mosi", "sck", "cs", "dc", "rst", "busy")


def button_count(board):
    return board.get("buttons") or len(board.get("button_pins", []))


def board_defines(board_id, board, features, version):
    """Return ([(name, string)], [(name, value)]) for CPPDEFINES; the strings get quoted by the caller."""
    driver = board.get("driver")
    if driver not in DRIVERS:
        raise ConfigError("driver", f"unknown driver {driver!r}; known: {list(DRIVERS)}")
    pins = board.get("button_pins", [])
    led = board.get("led") or {}
    strings = [("APP_BOARD_ID", board_id), ("BOARD_LABEL", board["label"]), ("FIRMWARE_VERSION", version)]
    plain = [(f"BOARD_DRIVER_{driver.upper()}", 1), ("BOARD_BUTTON_COUNT", button_count(board))]
    if pins:
        plain.append(("BOARD_BUTTON_PINS", "{" + ",".join(map(str, pins)) + "}"))
    plain += [("BOARD_LED_PIN", led.get("pin", -1)), ("BOARD_LED_RGB", int(led.get("rgb", False))),
              ("BOARD_HAS_DISPLAY", int(board.get("display", False))),
              ("BOARD_HAS_BATTERY", int(board.get("battery", False)))]
    power = board.get("power")
    if power:
        plain += [("BOARD_POWER_PIN", power["pin"]), ("BOARD_POWER_ACTIVE_LOW", int(power.get("active_low", False)))]
    epaper = board.get("epaper")
    if epaper:
        panel = epaper.get("panel")
        if panel not in PANELS:
            raise ConfigError("epaper.panel", f"unknown panel {panel!r}; known: {list(PANELS)}")
        plain += [(f"BOARD_EPD_{name.upper()}", epaper[name]) for name in EPD_PINS]
        plain.append((f"BOARD_EPD_PANEL_{panel.upper()}", 1))
    plain += [(name, int(on)) for name, on in sorted(features.items())]
    return strings, plain
