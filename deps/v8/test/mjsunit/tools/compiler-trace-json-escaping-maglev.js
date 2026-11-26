// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-maglev

// 1. \v (0x0b) and \f (0x0c) in source text (AsEscapedUC16ForJSON).
const test_maglev_source_vt_ff = eval(
    '(function test_maglev_source_vt_ff(x) {\x0b let s = x + 1;\x0c return s; })');
%PrepareFunctionForOptimization(test_maglev_source_vt_ff);
test_maglev_source_vt_ff(1);
test_maglev_source_vt_ff(2);
%OptimizeMaglevOnNextCall(test_maglev_source_vt_ff);
test_maglev_source_vt_ff(3);

// 2. Object literal and class method names with quotes, backslashes,
// newlines, and control characters, including Maglev inlined deopt frames.
const methodHolder = {
  'lit"fn\n\x01'(x) {
    return x + 1;
  }
};
class MethodClass {
  'cls"fn\n\x01'(x) {
    return x + 2;
  }
}
const methodInstance = new MethodClass();
function test_maglev_inlined_names(x) {
  return methodHolder['lit"fn\n\x01'](x) +
      methodInstance['cls"fn\n\x01'](x);
}
%PrepareFunctionForOptimization(methodHolder['lit"fn\n\x01']);
%PrepareFunctionForOptimization(MethodClass.prototype['cls"fn\n\x01']);
%PrepareFunctionForOptimization(test_maglev_inlined_names);
test_maglev_inlined_names(1);
test_maglev_inlined_names(2);
%OptimizeMaglevOnNextCall(test_maglev_inlined_names);
test_maglev_inlined_names(3);

// 3. UTF-8 characters in JS function name (Maglev disassembly).
const utf8Holder = {
  'test_maglev_caf\u00e9'(x) {
    return x + 1;
  }
};
const utf8Fn = utf8Holder['test_maglev_caf\u00e9'];
%PrepareFunctionForOptimization(utf8Fn);
utf8Fn(1);
utf8Fn(2);
%OptimizeMaglevOnNextCall(utf8Fn);
utf8Fn(3);
