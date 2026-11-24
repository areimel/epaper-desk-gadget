"""Tests for scripts/embed_ui.py: collection, compression, rendering, limits."""
import gzip
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from support import ROOT  # noqa: F401 -- sets up sys.path
import embed_ui
from embed_ui import EmbedError, build, collect, mime_for, render


def make_tree(root, names):
    for name in names:
        path = Path(root) / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"content of " + name.encode())


class TempCase(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.src = self.tmp / "device-ui"
        self.src.mkdir()


class CollectTests(TempCase):
    def test_exclusions_and_nesting(self):
        make_tree(self.src, ["index.html", "app/main.js", "mock/a.html", "tests/t.js",
                             "tools/x.py", ".hidden", ".git/config", "app/.cache/x.js",
                             "README.md", "app/notes.md"])
        self.assertEqual([p for p, _ in collect(self.src)], ["app/main.js", "index.html"])

    def test_allow_list_matches_web_app_layout(self):
        make_tree(self.src, ["index.html", "styles/app.css", "js/main.js", "fonts/a.woff2",
                             "fonts/OFL.txt", "package.json", "package-lock.json", "THIRD_PARTY.md",
                             "icon.svg", "favicon.ico", "img/x.png", "notes.json", "x.mjs", "y.bin"])
        self.assertEqual([p for p, _ in collect(self.src)],
                         ["favicon.ico", "fonts/a.woff2", "icon.svg", "img/x.png", "index.html",
                          "js/main.js", "styles/app.css"])

    def test_nested_dir_named_mock_is_included(self):
        make_tree(self.src, ["app/mock/a.js"])
        self.assertEqual([p for p, _ in collect(self.src)], ["app/mock/a.js"])

    def test_secret_path_raises(self):
        make_tree(self.src, ["app/My-Secrets.js"])
        with self.assertRaises(EmbedError):
            collect(self.src)

    def test_missing_and_empty_directory(self):
        self.assertEqual(collect(self.tmp / "nope"), [])
        self.assertEqual(collect(self.src), [])


class MimeTests(unittest.TestCase):
    def test_table(self):
        cases = {
            "a.html": ("text/html; charset=utf-8", True), "a.css": ("text/css", True),
            "a.js": ("text/javascript", True), "a.JS": ("text/javascript", True),
            "a.svg": ("image/svg+xml", True), "a.woff2": ("font/woff2", False),
            "a.png": ("image/png", False), "a.ico": ("image/x-icon", False),
            "a.bin": ("application/octet-stream", False), "noext": ("application/octet-stream", False),
        }
        for name, expected in cases.items():
            self.assertEqual(mime_for(name), expected, name)


class RenderTests(TempCase):
    def test_gzip_round_trip(self):
        data = b"<html>" + b"hello " * 100 + b"</html>"
        payload, mime, gz = embed_ui.embedded_bytes("index.html", data)
        self.assertTrue(gz)
        self.assertEqual(gzip.decompress(payload), data)

    def test_binary_types_not_compressed(self):
        for name in ("f.woff2", "i.png"):
            payload, _, gz = embed_ui.embedded_bytes(name, b"\x00raw")
            self.assertFalse(gz)
            self.assertEqual(payload, b"\x00raw")

    def test_header_shape(self):
        text = render([("index.html", b"<p>hi</p>"), ("a/i.png", b"\x89PNG")])
        self.assertIn('{"/index.html", "text/html; charset=utf-8", UI_DATA_0,', text)
        self.assertIn('{"/a/i.png", "image/png", UI_DATA_1, 4, false}', text)
        self.assertIn("UI_FILE_COUNT = 2;", text)
        self.assertIn("UI_TOTAL_BYTES = ", text)

    def test_etag_follows_content(self):
        a = render([("index.html", b"<p>one</p>")])
        b = render([("index.html", b"<p>two</p>")])
        tag = [line for line in a.splitlines() if "UI_ETAG" in line]
        self.assertRegex(tag[0], r'^static const char UI_ETAG\[\] = "[0-9a-f]{16}";$')
        self.assertNotIn(tag[0], b)
        self.assertIn(tag[0], render([("index.html", b"<p>one</p>")]))

    def test_path_escaping(self):
        text = render([('a"b\\c.html', b"x")])
        self.assertIn(r'"/a\"b\\c.html"', text)

    def test_empty(self):
        text = render([])
        self.assertIn("UI_FILE_COUNT = 0;", text)
        self.assertIn("UI_TOTAL_BYTES = 0;", text)

    def test_size_guard(self):
        big = [("blob.bin", os.urandom(embed_ui.MAX_TOTAL_BYTES + 1))]
        with self.assertRaises(EmbedError) as ctx:
            render(big)
        self.assertIn(str(embed_ui.MAX_TOTAL_BYTES), str(ctx.exception))
        with mock.patch.object(embed_ui, "MAX_TOTAL_BYTES", 10):
            with self.assertRaises(EmbedError):
                render([("a.png", b"x" * 11)])


class BuildTests(TempCase):
    def test_deterministic_and_unchanged(self):
        make_tree(self.src, ["index.html", "app.js", "logo.png"])
        a, b = self.tmp / "o1" / "ui.h", self.tmp / "o2" / "ui.h"
        self.assertEqual(build(self.src, a)[0], 3)
        self.assertTrue(build(self.src, b)[2])
        self.assertEqual(a.read_bytes(), b.read_bytes())
        self.assertFalse(build(self.src, a)[2])

    def test_empty_build(self):
        count, total, changed = build(self.tmp / "missing", self.tmp / "ui.h")
        self.assertEqual((count, total, changed), (0, 0, True))
        self.assertIn("UI_FILE_COUNT = 0", (self.tmp / "ui.h").read_text())


@unittest.skipUnless(shutil.which("g++") or shutil.which("c++"), "no C++ compiler")
class CompileTests(TempCase):
    def check(self, files):
        header = self.tmp / "ui_files.h"
        make_tree(self.src, files)
        build(self.src, header)
        cpp = self.tmp / "t.cpp"
        cpp.write_text('#include "ui_files.h"\nsize_t f() { return UI_FILES[0].len + UI_FILE_COUNT; }\n')
        result = subprocess.run([shutil.which("g++") or shutil.which("c++"), "-std=gnu++17", "-fsyntax-only",
                                 "-x", "c++", "-I", str(self.tmp), str(cpp)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_empty_compiles(self):
        self.check([])

    def test_two_files_compile(self):
        self.check(["index.html", "img/logo.png"])


if __name__ == "__main__":
    unittest.main()
