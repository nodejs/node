// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Test string addition with an object that has a custom toString method.
{
  const obj = { toString: () => "custom" };

  function foo(a, b) {
    return a + b;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals("custom_suffix", foo(obj, "_suffix"));
  assertEquals("prefix_custom", foo("prefix_", obj));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("custom_suffix", foo(obj, "_suffix"));
  assertEquals("prefix_custom", foo("prefix_", obj));
}

// Test string addition with null and undefined.
{
  function foo(a, b) {
    return a + b;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals("null_suffix", foo(null, "_suffix"));
  assertEquals("prefix_null", foo("prefix_", null));
  assertEquals("undefined_suffix", foo(undefined, "_suffix"));
  assertEquals("prefix_undefined", foo("prefix_", undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("null_suffix", foo(null, "_suffix"));
  assertEquals("prefix_null", foo("prefix_", null));
  assertEquals("undefined_suffix", foo(undefined, "_suffix"));
  assertEquals("prefix_undefined", foo("prefix_", undefined));
}

// Test string addition with BigInts.
{
  const b = 123n;
  function foo(a, b) {
    return a + b;
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals("123_suffix", foo(b, "_suffix"));
  assertEquals("prefix_123", foo("prefix_", b));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("123_suffix", foo(b, "_suffix"));
  assertEquals("prefix_123", foo("prefix_", b));
}

// Test string addition with Symbols, which should throw.
{
  const s = Symbol('foo');
  function foo(a, b) {
    return a + b;
  }

  %PrepareFunctionForOptimization(foo);
  assertThrows(() => foo(s, "_suffix"), TypeError);
  assertThrows(() => foo("prefix_", s), TypeError);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(() => foo(s, "_suffix"), TypeError);
  assertThrows(() => foo("prefix_", s), TypeError);
}

// Test with an object that has a [@@toPrimitive] method.
{
  const obj = {
    [Symbol.toPrimitive](hint) {
      if (hint === 'number') {
        return 42;
      }
      if (hint === 'string') {
        return 'primitive';
      }
      return 'default';
    }
  };

  function foo(a, b) {
    return a + b;
  }

  %PrepareFunctionForOptimization(foo);
  // The '+' operator uses a 'default' hint for ToPrimitive.
  assertEquals("default_suffix", foo(obj, "_suffix"));
  assertEquals("prefix_default", foo("prefix_", obj));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("default_suffix", foo(obj, "_suffix"));
  assertEquals("prefix_default", foo("prefix_", obj));
}
