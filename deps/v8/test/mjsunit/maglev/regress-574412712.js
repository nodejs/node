// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

const o = {abc: 1};

function f1(s) {
  let c = s + s;
  if (s) {
    o[s];
    c + "x";
  }
  return c + "abcdefg";
}

%PrepareFunctionForOptimization(f1);
f1("abc");
%OptimizeMaglevOnNextCall(f1);
// If `f1("")` returns an invalid `ConsString("", "abcdefg")` with length 7
// (< ConsString::kMinLength), concatenating `"12345"` (total length 12 < 13)
// enters the short-string fast path in `NewConsString` that assumes direct
// string inputs.
assertEquals("abcdefg12345", f1("") + "12345");
assertMaglevved(f1);

const re = /(?=(a))$/;
re.test("a");

function f2(s, b) {
  let c = s + s;
  if (s) {
    o[s];
    c + "x";
  }
  return "0123456789abc" + (b + "x") + (b + "x") + c;
}

%PrepareFunctionForOptimization(f2);
f2("abc", "x");
%OptimizeMaglevOnNextCall(f2);
// Verify that `f2("", "x")` does not produce a `ConsString` with an empty
// `second` and an unflattened `ConsString` `first`, which `String::Flatten`
// would treat as already flat when passing the subject to RegExp execution.
assertEquals("0123456789abcxxxx", f2("", "x").replace(re, () => ""));
assertMaglevved(f2);
