#!/usr/bin/env python3
# vim:fenc=utf-8:ts=2:sw=2:softtabstop=2:expandtab:
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import re

liftoff_regex = re.compile('^Compiled function .* using Liftoff, '
                           '.*bodysize ([0-9]+) codesize ([0-9]+)$')
turbofan_regex = re.compile('^Compiled function .* using TurboFan, '
                            '.*bodysize ([0-9]+) codesize ([0-9]+) ')
wasm2js_regex = re.compile('^Compiled WasmToJS wrapper .* '
                           'codesize ([0-9]+)$')


def main():
  print('Reading --trace-wasm-compilation-times lines from stdin...')
  liftoff_values = []
  turbofan_values = []
  wasm2js_values = []
  for line in sys.stdin:
    match(line, liftoff_regex, liftoff_values)
    match(line, turbofan_regex, turbofan_values)
    match_wasm2js(line, wasm2js_values)

  evaluate('Liftoff', liftoff_values)
  evaluate('TurboFan', turbofan_values)
  evaluate_wasm2js(wasm2js_values)


def match(line, regex, array):
  m = regex.match(line)
  if m:
    array.append([int(m.group(1)), int(m.group(2))])


def match_wasm2js(line, array):
  m = wasm2js_regex.match(line)
  if m:
    array.append(int(m.group(1)))


def evaluate(name, values):
  n = len(values)
  if n == 0:
    print(f'No values for {name}')
    return

  print(f'Computing base and factor for {name} based on {n} values')
  sum_xy = sum(x * y for [x, y] in values)
  sum_x = sum(x for [x, y] in values)
  sum_y = sum(y for [x, y] in values)
  sum_xx = sum(x * x for [x, y] in values)

  factor = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x)
  base = (sum_y - factor * sum_x) / n

  print(f'--> [{name}] Trend line: base: {base:.2f}, factor {factor:.2f}')

  min_y = min(y for [x, y] in values)

  simple_factor = (sum_y - n * min_y) / sum_x
  print(f'--> [{name}] Simple analysis: Min {min_y}, '
        f'factor {simple_factor:.2f}')


def evaluate_wasm2js(values):
  n = len(values)
  if n == 0:
    print('No wasm2js wrappers')
    return

  print(f'--> [Wasm2js wrappers] {n} compiled, size min {min(values)}, '
        f'max {max(values)}, avg {(sum(values) / n):.2f}')


main()
