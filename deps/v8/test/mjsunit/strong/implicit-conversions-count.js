// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

"use strict";

function pre_inc(x) {
  return ++x;
}

function post_inc(x) {
  return x++;
}

function pre_dec(x) {
  return --x;
}

function post_dec(x) {
  return x--;
}

function getTestFuncs() {
  return [
    function(x){
      "use strong";
      let y = x;
      assertEquals(++y, pre_inc(x));
      try {
        assertEquals(x+1, y)
      } catch (e) {
        assertUnreachable();
      }
    },
    function(x){
      "use strong";
      let y = x;
      assertEquals(y++, post_inc(x));
      try {
        assertEquals(x+1, y)
      } catch (e) {
        assertUnreachable();
      }
    },
    function(x){
      "use strong";
      let y = x;
      assertEquals(--y, pre_dec(x));
      try {
        assertEquals(x-1, y)
      } catch (e) {
        assertUnreachable();
      }
    },
    function(x){
      "use strong";
      let y = x;
      assertEquals(y--, post_dec(x));
      try {
        assertEquals(x-1, y)
      } catch (e) {
        assertUnreachable();
      }
    },
    function(x){
      "use strong";
      let obj = { foo: x };
      let y = ++obj.foo;
      assertEquals(y, pre_inc(x));
      try {
        assertEquals(x+1, obj.foo)
      } catch (e) {
        assertUnreachable();
      }
    },
    function(x){
      "use strong";
      let obj = { foo: x };
      let y = obj.foo++;
      assertEquals(y, post_inc(x));
      try {
        assertEquals(x+1, obj.foo)
      } catch (e) {
        assertUnreachable();
      }
    },
    function(x){
      "use strong";
      let obj = { foo: x };
      let y = --obj.foo;
      assertEquals(y, pre_dec(x));
      try {
        assertEquals(x-1, obj.foo)
      } catch (e) {
        assertUnreachable();
      }
    },
    function(x){
      "use strong";
      let obj = { foo: x };
      let y = obj.foo--;
      assertEquals(y, post_dec(x));
      try {
        assertEquals(x-1, obj.foo)
      } catch (e) {
        assertUnreachable();
      }
    },
  ];
}

let nonNumberValues = [
  {},
  (function(){}),
  [],
  (class Foo {}),
  "",
  "foo",
  "NaN",
  Object(""),
  false,
  null,
  undefined
];

// Check prior input of None works
for (let func of getTestFuncs()) {
  for (let value of nonNumberValues) {
    assertThrows(function(){func(value)}, TypeError);
    assertThrows(function(){func(value)}, TypeError);
    assertThrows(function(){func(value)}, TypeError);
    %OptimizeFunctionOnNextCall(func);
    assertThrows(function(){func(value)}, TypeError);
    %DeoptimizeFunction(func);
  }
}

// Check prior input of Smi works
for (let func of getTestFuncs()) {
  func(1);
  func(1);
  func(1);
  for (let value of nonNumberValues) {
    assertThrows(function(){func(value)}, TypeError);
    assertThrows(function(){func(value)}, TypeError);
    assertThrows(function(){func(value)}, TypeError);
    %OptimizeFunctionOnNextCall(func);
    assertThrows(function(){func(value)}, TypeError);
    %DeoptimizeFunction(func);
  }
}

// Check prior input of Number works
for (let func of getTestFuncs()) {
  func(9999999999999);
  func(9999999999999);
  func(9999999999999);
  for (let value of nonNumberValues) {
    assertThrows(function(){func(value)}, TypeError);
    assertThrows(function(){func(value)}, TypeError);
    assertThrows(function(){func(value)}, TypeError);
    %OptimizeFunctionOnNextCall(func);
    assertThrows(function(){func(value)}, TypeError);
    %DeoptimizeFunction(func);
  }
}
