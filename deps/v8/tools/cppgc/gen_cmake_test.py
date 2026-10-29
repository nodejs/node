#!/usr/bin/env python3
#
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gen_cmake import CMakeBuilder, V8GNTransformer, ParseGN, V8_TARGET_TYPES
import unittest


class CMakeMockBuilder(CMakeBuilder):
    """
    Similar to CMakeBuilder but doesn't produce prologues/epilogues.
    """
    def BuildPrologue(self):
        pass

    def BuildEpilogue(self):
        pass


class CMakeGenerationTest(unittest.TestCase):
    TARGET = 'cppgc_base'
    CMAKE_TARGET_SOURCES = TARGET.upper() + '_SOURCES'

    def test_source_assignment(self):
        self._CompileAndCheck(
            f'set({self.CMAKE_TARGET_SOURCES} "source1.h" "source1.cc")',
            'sources = [ "source1.h", "source1.cc", ]')

    def test_source_append(self):
        self._CompileAndCheck(
            f'list(APPEND {self.CMAKE_TARGET_SOURCES} "source1.h" "source1.cc")',
            'sources += [ "source1.h", "source1.cc", ]')

    def test_source_remove(self):
        self._CompileAndCheck(
            f'list(REMOVE_ITEM {self.CMAKE_TARGET_SOURCES} "source1.h" "source1.cc")',
            'sources -= [ "source1.h", "source1.cc", ]')

    def test_equal(self):
        self._CompileExpressionAndCheck('"${CURRENT_CPU}" STREQUAL "x64"',
                                        'current_cpu == "x64"')

    def test_not_equal(self):
        self._CompileExpressionAndCheck('NOT "${CURRENT_CPU}" STREQUAL "x86"',
                                        'current_cpu != "x86"')

    def test_comparison_ops(self):
        OPS = {
            '<': 'LESS',
            '<=': 'LESS_EQUAL',
            '>': 'GREATER',
            '>=': 'GREATER_EQUAL',
        }
        for gn_op, cmake_op in OPS.items():
            self._CompileExpressionAndCheck(
                f'"${{GCC_VERSION}}" {cmake_op} 40802',
                f'gcc_version {gn_op} 40802')

    def test_parenthesized_expressions(self):
        self._CompileExpressionAndCheck(
            '(("${IS_POSIX}" AND NOT "${IS_ANDROID}") OR "${IS_FUCHSIA}") AND NOT "${USING_SANITIZER}"',
            '((is_posix && !is_android) || is_fuchsia) && !using_sanitizer')

    def test_conditional_statements(self):
        self._CompileAndCheck(
            f"""
if("${{IS_POSIX}}")
  list(APPEND {self.CMAKE_TARGET_SOURCES} "unistd.h")
else()
  list(REMOVE_ITEM {self.CMAKE_TARGET_SOURCES} "unistd.h")
endif()
            """, """
if (is_posix) {
  sources += ["unistd.h"]
} else {
  sources -= ["unistd.h"]
}
            """)

    def test_conditional_statements_elseif(self):
        self._CompileAndCheck(
            f"""
if("${{IS_POSIX}}")
  list(APPEND {self.CMAKE_TARGET_SOURCES} "unistd.h")
elseif("${{IS_WIN}}")
  list(REMOVE_ITEM {self.CMAKE_TARGET_SOURCES} "unistd.h")
endif()
            """, """
if (is_posix) {
  sources += ["unistd.h"]
} else if (is_win) {
  sources -= ["unistd.h"]
}
            """)

    def _Compile(self, gn_string):
        gn_code = f'v8_component({self.TARGET}) {{ {gn_string} }}'
        tree = ParseGN(gn_code)
        builder = CMakeMockBuilder()
        V8GNTransformer(builder, [self.TARGET]).Traverse(tree)
        return builder.GetResult()

    def _CompileAndCheck(self, expected_cmake, gn_string):
        actual_cmake = self._Compile(gn_string)
        self.assertIn(self._Canonicalize(expected_cmake),
                      self._Canonicalize(actual_cmake))
        pass

    def _CompileExpressionAndCheck(self, expected_cmake, gn_string):
        gn_string = f'if ({gn_string}) {{ sources = [ "source.cc" ] }}'
        expected_cmake = f'if({expected_cmake})'
        actual_cmake = self._Compile(gn_string)
        self.assertIn(self._Canonicalize(expected_cmake),
                      self._Canonicalize(actual_cmake))
        pass

    @staticmethod
    def _Canonicalize(str):
        return ' '.join(str.split()).strip()


if __name__ == '__main__':
    unittest.main()
