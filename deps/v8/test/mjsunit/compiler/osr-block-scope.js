// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

"use strict";

function nest(body, name, depth) {
  var header = "";
  for (var i = 0; i < depth; i++) {
    var x = "x" + (i + 1);
    header += "  for(var " + x + " = 0; " + x + " < 2; " + x + " = " + x + " + 1 | 0) {\n";
    body = body + "}"
  }

  return body.replace(new RegExp("function " + name + "\\(\\) {"),
                      "function " + name + "_" + x + "() {\n" + header);
}

function test(expected, func, depth) {
  assertEquals(expected, func());
  assertEquals(expected, func());
  assertEquals(expected, func());

  var orig = func.toString();
  var name = func.name;
  for (var depth = 1; depth < 4; depth++) {
    var body = nest(orig, name, depth);
    func = eval("(" + body + ")");

    assertEquals(expected, func());
    assertEquals(expected, func());
    assertEquals(expected, func());
  }
}

function foo() {
  var result;
  {
    let sum = 0;
    for (var i = 0; i < 10; i++) {
      %OptimizeOsr();
      sum += i;
    }
    result = sum;
  }
  return result;
}

test(45, foo);

function bar() {
  let sum = 0;
  for (var i = 0; i < 10; i++) {
    %OptimizeOsr();
    sum += i;
  }
  return sum;
}

test(45, bar);

function bon() {
  {
    let sum = 0;
    for (var i = 0; i < 10; i++) {
      if (i == 5) %OptimizeOsr();
      sum += i;
    }
    return sum;
  }
}

test(45, bon);

function row() {
  var i = 0;
  {
    let sum = 0;
    while (true) {
      if (i == 8) return sum;
      %OptimizeOsr();
      sum = i;
      i = i + 1 | 0;
    }
  }
  return 11;
}

test(7, row);

function nub() {
  let i = 0;
  while (i < 2) {
    %OptimizeOsr();
    i++;
  }
  return i;
}

test(2, nub);

function kub() {
  var result = 0;
  let i = 0;
  while (i < 2) {
    let x = i;
    %OptimizeOsr();
    i++;
    result = x;
  }
  return result;
}

test(1, kub);
