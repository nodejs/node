// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var g_eval = eval;
function emit_f(size) {
  var body = "function f(x) {" +
             "  if (x < 0) return x;" +
             "  var a = [1];" +
             "  if (x > 0) return [";
  for (var i = 0; i < size; i++) {
    body += "0.1, ";
  }
  body += "  ];" +
          "  return a;" +
          "}";
  g_eval(body);
}

// Length must be big enough to make the backing store's size not fit into
// a single instruction's immediate field (2^12).
var kLength = 701;
emit_f(kLength);
%PrepareFunctionForOptimization(f);
f(1);
f(1);
%OptimizeFunctionOnNextCall(f);
var a = f(1);

// Allocating something else should not disturb |a|.
var b = new Object();
for (var i = 0; i < kLength; i++) {
  assertEquals(0.1, a[i]);
}

// Allocating more should not crash.
for (var i = 0; i < 300; i++) {
  f(1);
}
