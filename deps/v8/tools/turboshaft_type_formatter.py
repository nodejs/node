# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

NAMESPACE_PATTERNS = [
    (r"v8::internal::compiler::turboshaft::", ""),
    (r"v8::internal::wasm::", "wasm::"),
    (r"v8::internal::", "i::"),
]

TURBOSHAFT_PATTERNS = [
  (r"[a-zA-Z_][a-zA-Z0-9_]*Reducer<\[\.\.\.\]>::ReduceInputGraphOperation<[a-z_A-Z0-9]+, [^<]+<", 1, False),
  (r"[a-zA-Z_][a-zA-Z0-9_]*Reducer<", 1, True),
  (r"UniformReducerAdapter<\[\.\.\.\]>::ReduceInputGraphOperation<[a-z_A-Z0-9]+, UniformReducerAdapter<", 1, False),
  (r"UniformReducerAdapter<\[\.\.\.\]>::ReduceOperation<\(Opcode\)\d+, UniformReducerAdapter<", 1, False),
  (r"UniformReducerAdapter<", 1, True),
  (r"OutputGraphAssembler<", 1, False),
  (r"[a-z_A-Z0-9]+Op::Explode<\[\.\.\.\]>\(OutputGraphAssembler<\[\.\.\.\]>::.*, OutputGraphAssembler<", 1, True),
  (r"[a-z_A-Z0-9]+Op::Explode<\[\.\.\.\]>\(OutputGraphAssembler<", 1, True),
  (r"[a-z_A-Z0-9]+Op::Explode<", 1, True),
  (r"GraphEmitter<", 1, False),
  (r"GraphVisitor<", 1, False),
]


class TurboshaftTypeFormatter:

  def __init__(self, anchor_line_start=True):
    self.namespace_patterns = [
        (re.compile(p[0]), p[1]) for p in NAMESPACE_PATTERNS
    ]

    anchor = r"^" if anchor_line_start else r"\b"
    self.patterns = [(anchor + p[0], p[1], p[2]) for p in TURBOSHAFT_PATTERNS]
    self.combined_pattern = re.compile("|".join(p[0] for p in self.patterns))

  def _starts_with_pattern(self, text):
    for p in self.patterns:
      match = re.match(p[0], text)
      if match:
        return match.group(0), p[1], p[2]
    return None

  def _skip_template_args(self, text, prefix, depth):
    if not text.startswith(prefix):
      return text
    if not prefix.endswith("<"):
      return text

    start_bracket = len(prefix)
    for i in range(start_bracket, len(text)):
      if text[i] == '<':
        depth += 1
      elif text[i] == '>':
        depth -= 1
        if depth == 0:
          return text[:start_bracket] + "[...]" + text[i:]
    return text

  def simplify_namespaces(self, text):
    # Simplify e.g., 'v8::internal::compiler::turboshaft::GraphEmitter' to 'ts::GraphEmitter'.
    for pattern, replacement in self.namespace_patterns:
      text = pattern.sub(replacement, text)
    return text

  def format_type(self, short_name):
    short_name = self.simplify_namespaces(short_name)

    # Simplify known Reducer, Visitor, Assembler, etc. patterns.
    last_pattern = None
    while True:
      pattern = self._starts_with_pattern(short_name)
      if not pattern:
        break
      if pattern == last_pattern:
        break
      prefix, depth, rerun = pattern
      short_name = self._skip_template_args(short_name, prefix, depth)
      if not rerun:
        break
      last_pattern = pattern
    return short_name
