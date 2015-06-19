// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

"use strict";

// TODO(conradw): Implement other strong operators
let strongNumberBinops = [
  "-",
  "*",
  "/",
  "%",
  "|",
  "&",
  "^",
  "<<",
  ">>",
  ">>>",
];

let strongStringOrNumberBinops = [
  "+"
];

let strongBinops = strongNumberBinops.concat(strongStringOrNumberBinops);

let strongUnops = [
  "~",
  "+",
  "-"
];

let nonStringOrNumberValues = [
  "{}",
  "false",
  "(function(){})",
  "[]",
  "(class Foo {})"
];

let stringValues = [
  "''",
  "'               '",
  "'foo'",
  "'f\\u006F\\u006F'",
  "'0'",
  "'NaN'"
];

let nonNumberValues = nonStringOrNumberValues.concat(stringValues);

let numberValues = [
  "0",
  "(-0)",
  "1",
  "0.79",
  "(-0.79)",
  "4294967295",
  "4294967296",
  "(-4294967295)",
  "(-4294967296)",
  "9999999999999",
  "(-9999999999999)",
  "1.5e10",
  "(-1.5e10)",
  "0xFFF",
  "(-0xFFF)",
  "NaN",
  "Infinity",
  "(-Infinity)"
];

function add_strong(x, y) {
  "use strong";
  return x + y;
}

function add_num_strong(x, y) {
  "use strong";
  return x + y;
}

function sub_strong(x, y) {
  "use strong";
  return x - y;
}

function mul_strong(x, y) {
  "use strong";
  return x * y;
}

function div_strong(x, y) {
  "use strong";
  return x / y;
}

function mod_strong(x, y) {
  "use strong";
  return x % y;
}

function or_strong(x, y) {
  "use strong";
  return x | y;
}

function and_strong(x, y) {
  "use strong";
  return x & y;
}

function xor_strong(x, y) {
  "use strong";
  return x ^ y;
}

function shl_strong(x, y) {
  "use strong";
  return x << y;
}

function shr_strong(x, y) {
  "use strong";
  return x >> y;
}

function sar_strong(x, y) {
  "use strong";
  return x >>> y;
}

function typed_add_strong(x, y) {
  "use strong";
  return (+x) + (+y);
}

function typed_sub_strong(x, y) {
  "use strong";
  return (+x) - (+y);
}

function typed_mul_strong(x, y) {
  "use strong";
  return (+x) * (+y);
}

function typed_div_strong(x, y) {
  "use strong";
  return (+x) / (+y);
}

function typed_mod_strong(x, y) {
  "use strong";
  return (+x) % (+y);
}

function typed_or_strong(x, y) {
  "use strong";
  return (+x) | (+y);
}

function typed_and_strong(x, y) {
  "use strong";
  return (+x) & (+y);
}

function typed_xor_strong(x, y) {
  "use strong";
  return (+x) ^ (+y);
}

function typed_shl_strong(x, y) {
  "use strong";
  return (+x) << (+y);
}

function typed_shr_strong(x, y) {
  "use strong";
  return (+x) >> (+y);
}

function typed_sar_strong(x, y) {
  "use strong";
  return (+x) >>> (+y);
}

let strongNumberFuncs = [add_num_strong, sub_strong, mul_strong, div_strong,
                         mod_strong, or_strong, and_strong, xor_strong,
                         shl_strong, shr_strong, sar_strong, typed_add_strong,
                         typed_sub_strong, typed_mul_strong, typed_div_strong,
                         typed_mod_strong, typed_or_strong,  typed_and_strong,
                         typed_xor_strong, typed_shl_strong, typed_shr_strong,
                         typed_sar_strong];

let strongStringOrNumberFuncs = [add_strong];

let strongFuncs = strongNumberFuncs.concat(strongStringOrNumberFuncs);

function assertStrongNonThrowBehaviour(expr) {
  assertEquals(eval(expr), eval("'use strong';" + expr));
  assertDoesNotThrow("'use strong'; " + expr + ";");
  assertDoesNotThrow("'use strong'; let v = " + expr + ";");
}

function assertStrongThrowBehaviour(expr) {
  assertDoesNotThrow("'use strict'; " + expr + ";");
  assertDoesNotThrow("'use strict'; let v = " + expr + ";");
  assertThrows("'use strong'; " + expr + ";", TypeError);
  assertThrows("'use strong'; let v = " + expr + ";", TypeError);
}

function checkArgumentCombinations(op, leftList, rightList, willThrow) {
  for (let v1 of leftList) {
    let assignExpr = "foo " + op + "= " + v1 + ";";
    for (let v2 of rightList) {
      let compoundAssignment = "'use strong'; let foo = " + v2 + "; " +
                               assignExpr;
      if(willThrow) {
        assertThrows(compoundAssignment, TypeError);
        assertStrongThrowBehaviour("(" + v1 + op + v2 + ")");
      } else {
        assertDoesNotThrow(compoundAssignment);
        assertStrongNonThrowBehaviour("(" + v1 + op + v2 + ")");
      }
    }
  }
}

for (let op of strongBinops) {
  checkArgumentCombinations(op, numberValues, numberValues, false);
  checkArgumentCombinations(op, numberValues, nonNumberValues, true);
}

for (let op of strongNumberBinops) {
  checkArgumentCombinations(op, nonNumberValues,
                            numberValues.concat(nonNumberValues), true);
}

for (let op of strongStringOrNumberBinops) {
  checkArgumentCombinations(op, nonNumberValues,
                            numberValues.concat(nonStringOrNumberValues), true);
  checkArgumentCombinations(op, nonStringOrNumberValues, stringValues, true);
  checkArgumentCombinations(op, stringValues, stringValues, false);
}

for (let op of strongUnops) {
  for (let value of numberValues) {
    assertStrongNonThrowBehaviour("(" + op + value + ")");
  }
  for (let value of nonNumberValues) {
    assertStrongThrowBehaviour("(" + op + value + ")");
  }
}

for (let func of strongNumberFuncs) {
  // Check IC None*None->None throws
  assertThrows(function(){func(2, "foo");}, TypeError);
  %OptimizeFunctionOnNextCall(func);
  assertThrows(function(){func(2, "foo");}, TypeError);
  %DeoptimizeFunction(func);
  func(4, 5);
  func(4, 5);
  // Check IC Smi*Smi->Smi throws
  assertThrows(function(){func(2, "foo");}, TypeError);
  %OptimizeFunctionOnNextCall(func);
  assertThrows(function(){func(2, "foo");}, TypeError);
  %DeoptimizeFunction(func);
  func(NaN, NaN);
  func(NaN, NaN);
  // Check IC Number*Number->Number throws
  assertThrows(function(){func(2, "foo");}, TypeError);
  %OptimizeFunctionOnNextCall(func);
  assertThrows(function(){func(2, "foo");}, TypeError);
  %DeoptimizeFunction(func);
}

for (let func of strongStringOrNumberFuncs) {
  // Check IC None*None->None throws
  assertThrows(function(){func(2, "foo");}, TypeError);
  %OptimizeFunctionOnNextCall(func);
  assertThrows(function(){func(2, "foo");}, TypeError);
  %DeoptimizeFunction(func);
  func("foo", "bar");
  func("foo", "bar");
  // Check IC String*String->String throws
  assertThrows(function(){func(2, "foo");}, TypeError);
  %OptimizeFunctionOnNextCall(func);
  assertThrows(function(){func(2, "foo");}, TypeError);
  %DeoptimizeFunction(func);
  func(NaN, NaN);
  func(NaN, NaN);
  // Check IC Generic*Generic->Generic throws
  assertThrows(function(){func(2, "foo");}, TypeError);
  %OptimizeFunctionOnNextCall(func);
  assertThrows(function(){func(2, "foo");}, TypeError);
  %DeoptimizeFunction(func);
}
