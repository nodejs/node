// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

"use strict";

function test(expected, func) {
  assertEquals(expected, func());
  assertEquals(expected, func());
  assertEquals(expected, func());
}

function bar() {
  var result;
  {
    let sum = 0;
    for (let i = 0; i < 90; i++) {
      sum += i;
      if (i == 45) %OptimizeOsr();
    }
    result = sum;
  }
  return result;
}

test(4005, bar);

function baz() {
  let sum = 0;
  for (let i = 0; i < 2; i++) {
    sum = 2;
    %OptimizeOsr();
  }
  return sum;
}

test(2, baz);

function qux() {
  var result = 0;
  for (let i = 0; i < 2; i++) {
    result = i;
    %OptimizeOsr();
  }
  return result;
}

test(1, qux);

function nux() {
  var result = 0;
  for (let i = 0; i < 2; i++) {
    {
      let sum = i;
      %OptimizeOsr();
      result = sum;
    }
  }
  return result;
}

test(1, nux);

function blo() {
  var result;
  {
    let sum = 0;
    for (let i = 0; i < 90; i++) {
      sum += i;
      if (i == 45) %OptimizeOsr();
    }
    result = ret;
    function ret() {
      return sum;
    }
  }
  return result;
}

test(4005, blo());
