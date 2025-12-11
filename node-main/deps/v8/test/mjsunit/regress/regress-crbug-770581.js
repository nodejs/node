// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(callback) {
  [Object].forEach(callback);
};
%PrepareFunctionForOptimization(f);
function message_of_f() {
  try {
    f("a teapot");
  } catch (e) {
    return String(e);
  }
}

assertEquals("TypeError: string \"a teapot\" is not a function", message_of_f());
assertEquals("TypeError: string \"a teapot\" is not a function", message_of_f());
%OptimizeFunctionOnNextCall(f);
assertEquals("TypeError: string \"a teapot\" is not a function", message_of_f());
