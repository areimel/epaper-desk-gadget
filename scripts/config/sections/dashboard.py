"""dashboard: compile-time defaults for the desk gadget (page dwell, starter messages).

Limits are read from lib/feed/src/Limits.h so the validator and the firmware cannot drift.
FEATURE_DASHBOARD also needs wifi.enabled; generate_config.render enforces that after
all sections ran, because a section cannot see another section's value.
"""
import re
from pathlib import Path

from config.core import ConfigError, boolean, cstr, fields

KEY, REQUIRED = "dashboard", False
DEFAULT = {"enabled": True}
LIMITS_HEADER = Path(__file__).resolve().parents[3] / "lib" / "feed" / "src" / "Limits.h"
WANTED = ("MESSAGES_MAX", "MESSAGE_TEXT_MAX", "MESSAGE_BY_MAX", "DWELL_MIN_S", "DWELL_MAX_S",
          "DWELL_DEFAULT_S")
_CONSTANT = re.compile(r"constexpr\s+[A-Za-z_][A-Za-z0-9_]*\s+([A-Z][A-Z0-9_]*)\s*=\s*(\d+)\s*;")


def parse_limits(text):
    """Return {NAME: int} for the constants this section needs; a missing one is an error."""
    found = {name: int(value) for name, value in _CONSTANT.findall(text)}
    missing = [name for name in WANTED if name not in found]
    if missing:
        raise ConfigError("dashboard", f"{LIMITS_HEADER} does not define {', '.join(missing)}")
    return {name: found[name] for name in WANTED}


def read_limits():
    try:
        return parse_limits(LIMITS_HEADER.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ConfigError("dashboard", f"cannot read {LIMITS_HEADER}: {exc.strerror}") from exc


def _bounded_text(value, path, limit, allow_empty):
    if not isinstance(value, str) or (not allow_empty and not value.strip()):
        raise ConfigError(path, "must be non-empty text" if not allow_empty else "must be text")
    if any(ord(c) < 0x20 or ord(c) == 0x7F for c in value):
        raise ConfigError(path, "must not contain control characters or line breaks")
    size = len(value.encode("utf-8"))
    if size > limit:
        raise ConfigError(path, f"is {size} bytes of UTF-8; the limit is {limit}")
    return value


def _message(item, index, limits):
    path = f"{KEY}.messages[{index}]"
    item = fields(item, ["text"], path, ["by"])
    return {"text": _bounded_text(item["text"], f"{path}.text", limits["MESSAGE_TEXT_MAX"], False),
            "by": _bounded_text(item.get("by", ""), f"{path}.by", limits["MESSAGE_BY_MAX"], True)}


def validate(value, ctx):
    limits = read_limits()
    value = fields(value, [], KEY, ["enabled", "dwell_s", "messages"])
    dwell = value.get("dwell_s", limits["DWELL_DEFAULT_S"])
    if type(dwell) is not int or not limits["DWELL_MIN_S"] <= dwell <= limits["DWELL_MAX_S"]:
        raise ConfigError("dashboard.dwell_s",
                          f"must be an integer between {limits['DWELL_MIN_S']} and {limits['DWELL_MAX_S']}")
    messages = value.get("messages", [])
    if not isinstance(messages, list):
        raise ConfigError("dashboard.messages", "must be a list of {text, by} entries")
    if len(messages) > limits["MESSAGES_MAX"]:
        raise ConfigError("dashboard.messages", f"has {len(messages)} entries; the limit is {limits['MESSAGES_MAX']}")
    return {"enabled": boolean(value.get("enabled", True), "dashboard.enabled"),
            "dwell_s": dwell,
            "messages": [_message(m, i, limits) for i, m in enumerate(messages)]}


def features(cfg, ctx):
    return {"FEATURE_DASHBOARD": cfg["enabled"]}


def emit(cfg, ctx):
    rows = [f"    {{{cstr(m['text'])}, {cstr(m['by'])}}}," for m in cfg["messages"]]
    return [f"constexpr uint16_t DEFAULT_DWELL_S = {cfg['dwell_s']};",
            "struct StarterMessage { const char* text; const char* by; };",
            "constexpr StarterMessage STARTER_MESSAGES[] = {",
            *(rows or ['    {"", ""},  // placeholder: zero-size arrays are not valid C++']),
            "};",
            f"constexpr uint8_t STARTER_MESSAGE_COUNT = {len(cfg['messages'])};"]
