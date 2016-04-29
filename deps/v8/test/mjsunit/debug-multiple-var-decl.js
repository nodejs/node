// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

// Test we break at every assignment in a var-statement with multiple
// variable declarations.

var exception = null;
var log = []

function f() {
  var l1 = 1,    // l
      l2,        // m
      l3 = 3;    // n
  let l4,        // o
      l5 = 5,    // p
      l6 = 6;    // q
  const l7 = 7,  // r
        l8 = 8,  // s
        l9 = 9;  // t
  return 0;      // u
}                // v

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    var col = exec_state.frame(0).sourceColumn();
    print(line);
    var match = line.match(/\/\/ (\w)$/);
    assertEquals(2, match.length);
    log.push(match[1] + col);
    if (match[1] != "v") {
      exec_state.prepareStep(Debug.StepAction.StepIn);
    }
  } catch (e) {
    exception = e;
  }
}

var Debug = debug.Debug;
Debug.setListener(listener);

debugger;        // a
var g1 = 1,      // b
    g2 = 2,      // c
    g3;          // d
let g4 = 4,      // e
    g5,          // f
    g6 = 6;      // g
const g7 = 7,    // h
      g8 = 8,    // i
      g9 = f();  // j

Debug.setListener(null);

assertNull(exception);

// Note that let declarations, if not explicitly initialized, implicitly
// initialize to undefined.

var expected = [
  "a0",               // debugger statement
  "b9","c9",          // global var
  "e9","f4","g9",     // global let
  "h11","i11","j11",  // global const
  "l11","n11",        // local var
  "o6","p11","q11",   // local let
  "r13","s13","t13",  // local const
  "u2","v0",          // return
];
assertEquals(expected, log);
