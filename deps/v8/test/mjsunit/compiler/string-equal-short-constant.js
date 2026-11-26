// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-externalize-string

function test(fn, expected) {
  const fresh = (s) => s.toUpperCase().toLowerCase();
  const twoByte = (s) => createExternalizableTwoByteString(fresh(s));
  assertFalse(isOneByteString(twoByte(expected)));

  %PrepareFunctionForOptimization(fn);
  for (let i = 0; i < 2; i++) {
    assertTrue(fn(expected));
    assertTrue(fn(fresh(expected)));
    assertTrue(fn(twoByte(expected)));
    assertFalse(fn(fresh(expected.slice(0, -1))));
    assertFalse(fn(fresh(expected + "!")));
    assertFalse(fn(fresh("X" + expected.slice(1))));
    assertFalse(fn(fresh(expected.slice(0, -1) + "X")));
    assertFalse(fn(null));
    %OptimizeFunctionOnNextCall(fn);
  }
}

test((x) => x === "a", "a");
test((x) => x === "if", "if");
test((x) => x === "for", "for");
test((x) => x === "else", "else");
test((x) => x === "while", "while");
test((x) => x === "return", "return");
test((x) => x === "default", "default");
test((x) => x === "function", "function");
test((x) => x === "instanceof", "instanceof");
test((x) => x === "0123456789abcdef", "0123456789abcdef");
// Also test with the constant on the left-hand side.
test((x) => "while" === x, "while");
