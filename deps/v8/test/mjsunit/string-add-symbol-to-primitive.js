// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// This test verifies that for the '+' operator, ToPrimitive is called on a
// Symbol before ToString throws a TypeError. We make this observable by
// wrapping the symbol in an object with a custom [Symbol.toPrimitive] method.
{
  let log = [];
  const sym = Symbol('my-symbol');

  const obj = {
    [Symbol.toPrimitive](hint) {
      log.push(`toPrimitive hint: ${hint}`);
      return sym;
    }
  };

  function add_string_and_symbol_obj(o) {
    return "prefix-" + o;
  }

  %PrepareFunctionForOptimization(add_string_and_symbol_obj);

  // Test non-optimized code.
  log = [];
  assertThrows(() => add_string_and_symbol_obj(obj), TypeError);
  assertEquals(['toPrimitive hint: default'], log);

  %OptimizeFunctionOnNextCall(add_string_and_symbol_obj);

  // Test optimized code.
  log = [];
  assertThrows(() => add_string_and_symbol_obj(obj), TypeError);
  assertEquals(['toPrimitive hint: default'], log);

  // Also test with the object on the left.
  function add_symbol_obj_and_string(o) {
    return o + "-suffix";
  }

  %PrepareFunctionForOptimization(add_symbol_obj_and_string);

  log = [];
  assertThrows(() => add_symbol_obj_and_string(obj), TypeError);
  assertEquals(['toPrimitive hint: default'], log);

  %OptimizeFunctionOnNextCall(add_symbol_obj_and_string);

  log = [];
  assertThrows(() => add_symbol_obj_and_string(obj), TypeError);
  assertEquals(['toPrimitive hint: default'], log);
}
