"""Regression tests for cpplint runtime/references and V8 Fast API.

Refs: https://github.com/nodejs/node/issues/45761
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import cpplint  # noqa: E402


def _lint(source):
    errors = []

    def collect(filename, linenum, category, confidence, message):
        errors.append((linenum, category, message))

    cpplint.ProcessFileData("test.cc", "cc", source.splitlines(), collect)
    return [e for e in errors if e[1] == "runtime/references"]


class FastApiCallbackOptionsRefTest(unittest.TestCase):
    def test_unqualified_fast_api_callback_options_not_flagged(self):
        # Mirrors src/node_buffer.cc usage after `using v8::FastApiCallbackOptions;`.
        src = (
            "namespace node {\n"
            "void FastFn(Local<Value> recv,\n"
            "            FastApiCallbackOptions& options) {\n"
            "}\n"
            "}  // namespace node\n"
        )
        self.assertEqual(_lint(src), [])

    def test_qualified_fast_api_callback_options_not_flagged(self):
        src = (
            "namespace node {\n"
            "void FastFn(Local<Value> recv,\n"
            "            v8::FastApiCallbackOptions& options) {\n"
            "}\n"
            "}  // namespace node\n"
        )
        self.assertEqual(_lint(src), [])

    def test_plain_non_const_reference_still_flagged(self):
        src = (
            "namespace node {\n"
            "void Bad(int& value) {\n"
            "}\n"
            "}  // namespace node\n"
        )
        errors = _lint(src)
        self.assertEqual(len(errors), 1, errors)
        self.assertIn("int& value", errors[0][2])

    def test_unrelated_type_with_similar_name_still_flagged(self):
        # Guard against an over-broad regex that would also accept e.g.
        # FastApiCallbackOptionsExtra&.
        src = (
            "namespace node {\n"
            "void Bad(FastApiCallbackOptionsExtra& opts) {\n"
            "}\n"
            "}  // namespace node\n"
        )
        errors = _lint(src)
        self.assertEqual(len(errors), 1, errors)


if __name__ == "__main__":
    unittest.main()
