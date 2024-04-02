# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import linux_perf_report as ipr
import StringIO
import unittest


PERF_SCRIPT_OUTPUT = """
# This line is a comment
# This should be ignored too
#
#    cdefab01  aRandomSymbol::Name(to, be, ignored)

   00000000 firstSymbol
   00000123 secondSymbol

   01234567 foo
   abcdef76 BytecodeHandler:bar
   76543210 baz

# Indentation shouldn't matter (neither should this line)

    01234567 foo
      abcdef76 BytecodeHandler:bar
        76543210 baz

      01234567 beep
   abcdef76 BytecodeHandler:bar
    76543210 baz

   01234567 hello
   abcdef76 v8::internal::Compiler
   00000000 Stub:CEntryStub
   76543210 world
   11111111 BytecodeHandler:nope

   00000000 Lost
   11111111 Builtin:InterpreterEntryTrampoline
   22222222 bar

   00000000 hello
   11111111 LazyCompile:~Foo

   11111111 Builtin:InterpreterEntryTrampoline
   22222222 bar
"""


class LinuxPerfReportTest(unittest.TestCase):
  def test_collapsed_callchains_generator(self):
    perf_stream = StringIO.StringIO(PERF_SCRIPT_OUTPUT)
    callchains = list(ipr.collapsed_callchains_generator(perf_stream))
    self.assertListEqual(callchains, [
      ['firstSymbol', 'secondSymbol', '[other]'],
      ["foo", "BytecodeHandler:bar", "[interpreter]"],
      ["foo", "BytecodeHandler:bar", "[interpreter]"],
      ["beep", "BytecodeHandler:bar", "[interpreter]"],
      ["hello", "v8::internal::Compiler", "Stub:CEntryStub", "[compiler]"],
      ["Lost", "[misattributed]"],
      ["hello", "LazyCompile:~Foo", "[jit]"],
      ["[entry trampoline]"],
    ])

  def test_collapsed_callchains_generator_hide_other(self):
    perf_stream = StringIO.StringIO(PERF_SCRIPT_OUTPUT)
    callchains = list(ipr.collapsed_callchains_generator(perf_stream,
                                                         hide_other=True,
                                                         hide_compiler=True,
                                                         hide_jit=True))
    self.assertListEqual(callchains, [
      ["foo", "BytecodeHandler:bar", "[interpreter]"],
      ["foo", "BytecodeHandler:bar", "[interpreter]"],
      ["beep", "BytecodeHandler:bar", "[interpreter]"],
      ["Lost", "[misattributed]"],
      ["[entry trampoline]"],
    ])

  def test_calculate_samples_count_per_callchain(self):
    counters = ipr.calculate_samples_count_per_callchain([
      ["foo", "BytecodeHandler:bar"],
      ["foo", "BytecodeHandler:bar"],
      ["beep", "BytecodeHandler:bar"],
      ["hello", "v8::internal::Compiler", "[compiler]"],
    ])
    self.assertItemsEqual(counters, [
      ('BytecodeHandler:bar;foo', 2),
      ('BytecodeHandler:bar;beep', 1),
      ('[compiler];v8::internal::Compiler;hello', 1),
    ])

  def test_calculate_samples_count_per_callchain(self):
    counters = ipr.calculate_samples_count_per_callchain([
      ["foo", "BytecodeHandler:bar"],
      ["foo", "BytecodeHandler:bar"],
      ["beep", "BytecodeHandler:bar"],
    ])
    self.assertItemsEqual(counters, [
      ('BytecodeHandler:bar;foo', 2),
      ('BytecodeHandler:bar;beep', 1),
    ])

  def test_calculate_samples_count_per_handler_show_compile(self):
    counters = ipr.calculate_samples_count_per_handler([
      ["foo", "BytecodeHandler:bar"],
      ["foo", "BytecodeHandler:bar"],
      ["beep", "BytecodeHandler:bar"],
      ["hello", "v8::internal::Compiler", "[compiler]"],
    ])
    self.assertItemsEqual(counters, [
      ("bar", 3),
      ("[compiler]", 1)
    ])

  def test_calculate_samples_count_per_handler_(self):
    counters = ipr.calculate_samples_count_per_handler([
      ["foo", "BytecodeHandler:bar"],
      ["foo", "BytecodeHandler:bar"],
      ["beep", "BytecodeHandler:bar"],
    ])
    self.assertItemsEqual(counters, [("bar", 3)])

  def test_multiple_handlers(self):
    perf_stream = StringIO.StringIO("""
        0000 foo(bar)
        1234 BytecodeHandler:first
        5678 a::random::call<to>(something, else)
        9abc BytecodeHandler:second
        def0 otherIrrelevant(stuff)
        1111 entrypoint
    """)
    callchains = list(ipr.collapsed_callchains_generator(perf_stream, False))
    self.assertListEqual(callchains, [
      ["foo", "BytecodeHandler:first", "[interpreter]"],
    ])

  def test_compiler_symbols_regex(self):
    compiler_symbols = [
      "v8::internal::Parser",
      "v8::internal::(anonymous namespace)::Compile",
      "v8::internal::Compiler::foo",
    ]
    for compiler_symbol in compiler_symbols:
      self.assertTrue(ipr.COMPILER_SYMBOLS_RE.match(compiler_symbol))

  def test_jit_code_symbols_regex(self):
    jit_code_symbols = [
      "LazyCompile:~Foo blah.js",
      "Eval:*",
      "Script:*Bar tmp.js",
    ]
    for jit_code_symbol in jit_code_symbols:
      self.assertTrue(ipr.JIT_CODE_SYMBOLS_RE.match(jit_code_symbol))

  def test_strip_function_parameters(self):
    def should_match(signature, name):
      self.assertEqual(ipr.strip_function_parameters(signature), name)

    should_match("foo(bar)", "foo"),
    should_match("Foo(foomatic::(anonymous)::bar(baz))", "Foo"),
    should_match("v8::(anonymous ns)::bar<thing(with, parentheses)>(baz, poe)",
       "v8::(anonymous ns)::bar<thing(with, parentheses)>")

if __name__ == '__main__':
    unittest.main()
