// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --disable-abortjs --disable-in-process-stack-traces

const count = 1048573; // Pushes close to the 2^20 limit
const filler = "()=>0;";

// Helper to create a large string of functions
function getFiller(n) {
  return filler.repeat(n);
}

// Test case 1: Arrow function in parameter
assertThrows(() => {
  let s = getFiller(count) + "(x = eval('')) => {}";
  new Function(s);
}, SyntaxError, "Too many eval calls in script");

// Test case 2: Nested arrow functions in parameters
assertThrows(() => {
  let s = getFiller(count - 1) + "(x = (y = eval('')) => {}) => {}";
  new Function(s);
}, SyntaxError, "Too many eval calls in script");

// Test case 3: Computed member name in class
assertThrows(() => {
  let s = getFiller(count) + "class C { [(() => eval(''))()] = 1 }";
  new Function(s);
}, SyntaxError, "Too many eval calls in script");
