// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Modified JSReceiver valueOf.
{
  RegExp.prototype.valueOf = function() { return 44; }

  function foo(a) {
    for (var i = 0; i < 100; ++i) {
      a = new RegExp(a);
    }
    return a;
  }

  function stuff() {
    return "" + foo("hello");
  }

  function test() {
    for (var i = 0; i < 100; ++i) {
      assertEquals(stuff(), "44");
    }
  }

  test();

  %PrepareFunctionForOptimization(stuff);
  %OptimizeMaglevOnNextCall(stuff);
  test();
}

// Modified StringWrapper valueOf.
{
  String.prototype.valueOf = function() { return 54; }

  function foo(a) {
    for (var i = 0; i < 100; ++i) {
      a = new String(a);
    }
    return a;
  }

  function stuff() {
    return "" + foo("hello");
  }

  function test() {
    for (var i = 0; i < 100; ++i) {
      assertEquals(stuff(), "54");
    }
  }

  test();

  %PrepareFunctionForOptimization(stuff);
  %OptimizeMaglevOnNextCall(stuff);
  test();
}
