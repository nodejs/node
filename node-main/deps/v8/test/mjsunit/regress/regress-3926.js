// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See: http://code.google.com/p/v8/issues/detail?id=3926

// Switch statements should disable hole check elimination

// Ensure that both reads and writes encounter the hole check
// FullCodeGen had an issue on reads; TurboFan had an issue on writes
function f(x) {
  var z;
  switch (x) {
    case 1:
      let y = 1;
    case 2:
      y = 2;
    case 3:
      z = y;
  }
  return z;
}
assertEquals(2, f(1));
assertThrows(function() {f(2)}, ReferenceError);
assertThrows(function() {f(3)}, ReferenceError);

// Ensure that hole checks are done even in subordinate scopes
assertThrows(function() {
  switch (1) {
    case 0:
      let x = 2;
    case 1:
    { // this block, plus the let below, adds another linear lexical scope
      let y = 3;
      x;
    }
  }
}, ReferenceError);

// Ensure that inner functions and eval don't skip hole checks

function g(x) {
  switch (x) {
    case 1:
      let z;
    case 2:
      return function() { z = 1; }
    case 3:
      return function() { return z; }
    case 4:
      return eval("z = 1");
    case 5:
      return eval("z");
  }
}

assertEquals(undefined, g(1)());
assertThrows(g(2), ReferenceError);
assertThrows(g(3), ReferenceError);
assertThrows(function () {g(4)}, ReferenceError);
assertThrows(function () {g(5)}, ReferenceError);

// Ensure the same in strict mode, with different eval and function semantics

function h(x) {
  'use strict'
  switch (x) {
    case 1:
      let z;
    case 2:
      return function() { z = 1; }
    case 3:
      return function() { return z; }
    case 4:
      return eval("z = 1");
    case 5:
      return eval("z");
  }
}

assertEquals(undefined, h(1)());
assertThrows(h(2), ReferenceError);
assertThrows(h(3), ReferenceError);
assertThrows(function () {h(4)}, ReferenceError);
assertThrows(function () {h(5)}, ReferenceError);
