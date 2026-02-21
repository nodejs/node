#!/usr/bin/env python3

"""Integration test"""

import os
import re
import shutil
import unittest

import gyp

fixture_dir = os.path.join(os.path.dirname(__file__), "fixtures")
gyp_file = os.path.join(os.path.dirname(__file__), "fixtures/integration.gyp")

supported_sysnames = {"darwin", "linux"}
sysname = os.uname().sysname.lower()
expected_dir = os.path.join(fixture_dir, f"expected-{sysname}")


class TestGyp(unittest.TestCase):
    def setUp(self) -> None:
        if sysname not in supported_sysnames:
            self.skipTest(f"Unsupported system: {sysname}")
        shutil.rmtree(os.path.join(fixture_dir, "out"), ignore_errors=True)

    def assert_file(self, actual, expected) -> None:
        actual_filepath = os.path.join(fixture_dir, actual)
        expected_filepath = os.path.join(expected_dir, expected)

        with open(expected_filepath) as in_file:
            expected_bytes = re.escape(in_file.read())
        expected_bytes = expected_bytes.replace("\\*", ".*")
        expected_re = re.compile(expected_bytes)

        with open(actual_filepath) as in_file:
            actual_bytes = in_file.read()

        try:
            self.assertRegex(actual_bytes, expected_re)
        except Exception:
            shutil.copyfile(actual_filepath, f"{expected_filepath}.actual")
            raise

    def test_ninja(self) -> None:
        rc = gyp.main(["-f", "ninja", "--depth", fixture_dir, gyp_file])
        assert rc == 0

        self.assert_file("out/Default/obj/test.ninja", "ninja/test.ninja")

    def test_make(self) -> None:
        rc = gyp.main(
            [
                "-f",
                "make",
                "--depth",
                fixture_dir,
                "--generator-output",
                "out",
                gyp_file,
            ]
        )
        assert rc == 0

        self.assert_file("out/test.target.mk", "make/test.target.mk")

    def test_cmake(self) -> None:
        rc = gyp.main(["-f", "cmake", "--depth", fixture_dir, gyp_file])
        assert rc == 0

        self.assert_file("out/Default/CMakeLists.txt", "cmake/CMakeLists.txt")
