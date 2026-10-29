#!/usr/bin/env python3

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the ninja.py file."""

import sys
import unittest
from pathlib import Path

from gyp.generator import ninja
from gyp.MSVSVersion import SelectVisualStudioVersion


def _has_visual_studio():
    """Check if Visual Studio can be detected by gyp's registry-based detection."""
    if not sys.platform.startswith("win"):
        return False
    try:
        SelectVisualStudioVersion("auto", allow_fallback=False)
        return True
    except ValueError:
        return False


class TestPrefixesAndSuffixes(unittest.TestCase):
    @unittest.skipUnless(
        _has_visual_studio(),
        "requires Windows with a Visual Studio installation detected via the registry",
    )
    def test_BinaryNamesWindows(self):
        writer = ninja.NinjaWriter(
            "foo", "wee", ".", ".", "build.ninja", ".", "build.ninja", "win"
        )
        spec = {"target_name": "wee"}
        for key, ext in {
            "executable": ".exe",
            "shared_library": ".dll",
            "static_library": ".lib",
        }.items():
            self.assertTrue(writer.ComputeOutputFileName(spec, key).endswith(ext))

    def test_BinaryNamesLinux(self):
        writer = ninja.NinjaWriter(
            "foo", "wee", ".", ".", "build.ninja", ".", "build.ninja", "linux"
        )
        spec = {"target_name": "wee"}
        self.assertTrue("." not in writer.ComputeOutputFileName(spec, "executable"))
        self.assertTrue(
            writer.ComputeOutputFileName(spec, "shared_library").startswith("lib")
        )
        self.assertTrue(
            writer.ComputeOutputFileName(spec, "static_library").startswith("lib")
        )
        self.assertTrue(
            writer.ComputeOutputFileName(spec, "shared_library").endswith(".so")
        )
        self.assertTrue(
            writer.ComputeOutputFileName(spec, "static_library").endswith(".a")
        )

    def test_GenerateCompileDBWithNinja(self):
        build_dir = (
            Path(__file__).resolve().parent.parent.parent.parent / "data" / "ninja"
        )
        compile_db = ninja.GenerateCompileDBWithNinja(build_dir)
        assert len(compile_db) == 1
        assert compile_db[0]["directory"] == str(build_dir)
        assert compile_db[0]["command"] == "cc my.in my.out"
        assert compile_db[0]["file"] == "my.in"
        assert compile_db[0]["output"] == "my.out"


if __name__ == "__main__":
    unittest.main()
