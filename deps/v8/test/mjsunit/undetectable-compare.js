// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var undetectable = %GetUndetectable();

var tests = [
  true,        false,
  false,       false,
  null,        true,
  void 0,      true,
  0,           false,
  0.0,         false,
  -0,          false,
  "",          false,
  -1,          false,
  -1.25,       false,
  1,           false,
  1.25,        false,
  -2147483648, false,
  2147483648,  false,
  Infinity,    false,
  -Infinity,   false,
  NaN,         false
];

var func = (function eq(a) { return a == undetectable; });
var left_funcs = [
  (function eq_L0() { return true == undetectable; }),
  (function eq_L1() { return false == undetectable; }),
  (function eq_L2() { return null == undetectable; }),
  (function eq_L3() { return void 0 == undetectable; }),
  (function eq_L4() { return 0 == undetectable; }),
  (function eq_L5() { return 0.0 == undetectable; }),
  (function eq_L6() { return -0 == undetectable; }),
  (function eq_L7() { return "" == undetectable; }),
  (function eq_L8() { return -1 == undetectable; }),
  (function eq_L9() { return -1.25 == undetectable; }),
  (function eq_L10() { return 1 == undetectable; }),
  (function eq_L11() { return 1.25 == undetectable; }),
  (function eq_L12() { return -2147483648 == undetectable; }),
  (function eq_L13() { return 2147483648 == undetectable; }),
  (function eq_L14() { return Infinity == undetectable; }),
  (function eq_L15() { return -Infinity == undetectable; }),
  (function eq_L16() { return NaN == undetectable; })
];

var right_funcs = [
  (function eq_R0() { return undetectable == true; }),
  (function eq_R1() { return undetectable == false; }),
  (function eq_R2() { return undetectable == null; }),
  (function eq_R3() { return undetectable == void 0; }),
  (function eq_R4() { return undetectable == 0; }),
  (function eq_R5() { return undetectable == 0.0; }),
  (function eq_R6() { return undetectable == -0; }),
  (function eq_R7() { return undetectable == ""; }),
  (function eq_R8() { return undetectable == -1; }),
  (function eq_R9() { return undetectable == -1.25; }),
  (function eq_R10() { return undetectable == 1; }),
  (function eq_R11() { return undetectable == 1.25; }),
  (function eq_R12() { return undetectable == -2147483648; }),
  (function eq_R13() { return undetectable == 2147483648; }),
  (function eq_R14() { return undetectable == Infinity; }),
  (function eq_R15() { return undetectable == -Infinity; }),
  (function eq_R16() { return undetectable == NaN; })
];

function test() {
  for (var i = 0; i < tests.length; i += 2) {
    var val = tests[i];
    var eq = tests[i + 1];

    assertEquals(eq, val == undetectable);
    assertEquals(eq, undetectable == val);

    assertFalse(val === undetectable);
    assertFalse(undetectable === val);

    assertEquals(eq, left_funcs[i/2]());
    assertEquals(eq, right_funcs[i/2]());
  }

  assertTrue(undetectable == undetectable);
  assertTrue(undetectable === undetectable);

}

for (var i = 0; i < 5; i++) {
  test();
}


assertFalse(undetectable == %GetUndetectable());
assertFalse(undetectable === %GetUndetectable());


function test2(a, b) {
  return a == b;
}
test2(0, 1);
test2(undetectable, {});
%OptimizeFunctionOnNextCall(test2);
for (var i = 0; i < 5; ++i) {
  assertFalse(test2(undetectable, %GetUndetectable()));
}
