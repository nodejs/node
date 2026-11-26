// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-turbo

// 1. \v (0x0b) and \f (0x0c) in source text (AsEscapedUC16ForJSON).
const test_source_vt_ff = eval(
    '(function test_source_vt_ff(x) {\x0b let s = x + 1;\x0c return s; })');
%PrepareFunctionForOptimization(test_source_vt_ff);
test_source_vt_ff(1);
test_source_vt_ff(2);
%OptimizeFunctionOnNextCall(test_source_vt_ff);
test_source_vt_ff(3);

// 2. Object literal and class method names with quotes, backslashes,
// newlines, and control characters (JsonPrintFunctionSource,
// JsonPrintBytecodeSource, FrameStateOp).
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
function test_method_names(x) {
  return methodHolder['lit"fn\n\x01'](x) +
      methodInstance['cls"fn\n\x01'](x);
}
%PrepareFunctionForOptimization(methodHolder['lit"fn\n\x01']);
%PrepareFunctionForOptimization(MethodClass.prototype['cls"fn\n\x01']);
%PrepareFunctionForOptimization(test_method_names);
test_method_names(1);
test_method_names(2);
%OptimizeFunctionOnNextCall(test_method_names);
test_method_names(3);

// 3. FeedbackVector keyed load holding string key with \r, \t, \x01, and ".
const fbObj = {};
const fbKey = 'fb"key\r\t\x01';
fbObj[fbKey] = 10;
function test_feedback_vector(o, key) {
  return o[key];
}
%PrepareFunctionForOptimization(test_feedback_vector);
test_feedback_vector(fbObj, fbKey);
test_feedback_vector(fbObj, fbKey);
%OptimizeFunctionOnNextCall(test_feedback_vector);
test_feedback_vector(fbObj, fbKey);

// 4. Turboshaft LoadDictionaryFieldOp with unescaped characters in property
// name (PrintTurboshaftCustomDataPerOperation).
const dictObj = {};
for (let i = 0; i < 200; i++) dictObj['k' + i] = i;
dictObj['dict"prop\n\x01'] = 42;
function test_dictionary_load(o) {
  return o['dict"prop\n\x01'];
}
%PrepareFunctionForOptimization(test_dictionary_load);
test_dictionary_load(dictObj);
test_dictionary_load(dictObj);
%OptimizeFunctionOnNextCall(test_dictionary_load);
test_dictionary_load(dictObj);

// 5. UTF-8 characters in JS function name (Turboshaft disassembly).
const utf8Holder = {
  'test_utf8_caf\u00e9'(x) {
    return x + 1;
  }
};
const utf8Fn = utf8Holder['test_utf8_caf\u00e9'];
%PrepareFunctionForOptimization(utf8Fn);
utf8Fn(1);
utf8Fn(2);
%OptimizeFunctionOnNextCall(utf8Fn);
utf8Fn(3);
