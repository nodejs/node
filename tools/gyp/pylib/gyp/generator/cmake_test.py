#!/usr/bin/env python3

"""Unit tests for the cmake.py file."""

import io
import types
import unittest

from gyp.generator import cmake


class TestCustomCommandComment(unittest.TestCase):
    def test_ActionMessageIsQuotedAndEscaped(self):
        output = io.StringIO()
        action = {
            "action_name": "gen",
            "inputs": ["in.txt"],
            "outputs": ["out.txt"],
            "action": ["python", "gen.py"],
            "message": 'Generating "out.txt"; (see #1)',
        }
        cmake.WriteActions("tgt", [action], [], [], ".", output)
        self.assertIn(
            '  COMMENT "Generating \\"out.txt\\"\\; (see #1)"\n', output.getvalue()
        )

    def test_ActionWithoutMessageUsesTargetName(self):
        output = io.StringIO()
        action = {
            "action_name": "gen",
            "inputs": [],
            "outputs": ["out.txt"],
            "action": ["python", "gen.py"],
        }
        cmake.WriteActions("tgt", [action], [], [], ".", output)
        self.assertIn('  COMMENT "tgt__gen"\n', output.getvalue())

    def test_RuleMessageKeepsVariableReferences(self):
        output = io.StringIO()
        rule = {
            "rule_name": "compile",
            "outputs": ["${RULE_INPUT_ROOT}.o"],
            "action": ["cc", "${RULE_INPUT_PATH}"],
            "rule_sources": ["foo.c"],
            "message": "Compiling ${RULE_INPUT_NAME}",
        }
        cmake.WriteRules("tgt", [rule], [], [], ".", output)
        self.assertIn('  COMMENT "Compiling ${RULE_INPUT_NAME}"\n', output.getvalue())

    def test_CopiesMessageIsQuoted(self):
        output = io.StringIO()
        copies = [{"files": ["a.txt"], "destination": "out"}]
        cmake.WriteCopies("tgt", copies, [], ".", output)
        self.assertIn('COMMENT "Copying for tgt"\n', output.getvalue())


class TestTargetWithoutSources(unittest.TestCase):
    def _WriteTarget(self, target_type, sources=()):
        qualified_target = "foo.gyp:foo#target"
        spec = {
            "target_name": "foo",
            "type": target_type,
            "toolset": "target",
            "sources": list(sources),
        }
        output = io.StringIO()
        cmake.WriteTarget(
            cmake.CMakeNamer([qualified_target]),
            qualified_target,
            {qualified_target: spec},
            "out/Default",
            "Default",
            types.SimpleNamespace(toplevel_dir="."),
            {},
            [qualified_target],
            "linux",
            output,
        )
        return output.getvalue()

    def test_DummySourceForEmptyTargets(self):
        for target_type, add_target in (
            ("executable", "add_executable(foo"),
            ("static_library", "add_library(foo STATIC"),
            ("shared_library", "add_library(foo SHARED"),
            ("loadable_module", "add_library(foo MODULE"),
        ):
            with self.subTest(target_type=target_type):
                output = self._WriteTarget(target_type)
                self.assertIn('  file(WRITE "${foo__dummy_srcs}" "")\n', output)
                self.assertIn(add_target + " ${foo__dummy_srcs})\n", output)

    def test_NoDummySourceForLibraryWithSources(self):
        output = self._WriteTarget("static_library", ["foo.c"])
        self.assertIn("add_library(foo STATIC ${foo__c_srcs})\n", output)
        self.assertNotIn("dummy", output)

    def test_NoDummySourceForNoneTarget(self):
        output = self._WriteTarget("none")
        self.assertIn("add_custom_target(foo SOURCES)\n", output)
        self.assertNotIn("dummy", output)


if __name__ == "__main__":
    unittest.main()
