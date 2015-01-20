// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --harmony

Debug = debug.Debug;
var break_count = 0
var exception = null;
var log = []

var s = 0;
var a = [1, 2, 3];
var i = 0;

function f() {
  "use strict";
  debugger;                      // Break a
  var j;                         // Break b

  for (var i in null) {          // Break c
    s += a[i];
  }

  for (j in null) {              // Break d
    s += a[j];
  }

  for (var i in a) {             // Break e
    s += a[i];                   // Break E
  }

  for (j in a) {                 // Break f
    s += a[j];                   // Break F
  }

  for (let i in a) {             // Break g
    s += a[i];                   // Break G
  }

  for (var i of a) {             // Break h
    s += i;                      // Break H
  }

  for (j of a) {                 // Break i
    s += j;                      // Break I
  }

  for (let i of a) {             // Break j
    s += i;                      // Break J
  }

  for (var i = 0; i < 3; i++) {  // Break k
    s += a[i];                   // Break K
  }

  for (j = 0; j < 3; j++) {      // Break l
    s += a[j];                   // Break L
  }

  // TODO(yangguo): add test case for for-let.
}                                // Break y

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    var col = exec_state.frame(0).sourceColumn();
    print(line);
    var match = line.match(/\/\/ Break (\w)$/);
    assertEquals(2, match.length);
    log.push(match[1] + col);
    exec_state.prepareStep(Debug.StepAction.StepNext, 1);
    break_count++;
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);
f();
Debug.setListener(null);         // Break z

print(JSON.stringify(log));
// The let declaration differs from var in that the loop variable
// is declared in every iteration.
var expected = [
  // Entry
  "a2","b2",
  // Empty for-in-var: var decl, get enumerable
  "c7","c16",
  // Empty for-in: get enumerable
  "d12",
  // For-in-var: var decl, get enumerable, assign, body, assign, body, ...
  "e7","e16","e11","E4","e11","E4","e11","E4","e11",
  // For-in: get enumerable, assign, body, assign, body, ...
  "f12","f7","F4","f7","F4","f7","F4","f7",
  // For-in-let: get enumerable, next, new let, body, next, new let, ...
  "g16","g11","g7","G4","g11","g7","G4","g11","g7","G4","g11",
  // For-of-var: var decl, next(), body, next(), body, ...
  "h7","h16","H4","h16","H4","h16","H4","h16",
  // For-of: next(), body, next(), body, ...
  "i12","I4","i12","I4","i12","I4","i12",
  // For-of-let: next(), new let, body, next(), new let, ...
  "j16","j7","J4","j16","j7","J4","j16","j7","J4","j16",
  // For-var: var decl, condition, body, next, condition, body, ...
  "k7","k20","K4","k23","k20","K4","k23","k20","K4","k23","k20",
  // For: init, condition, body, next, condition, body, ...
  "l11","l16","L4","l19","l16","L4","l19","l16","L4","l19","l16",
  // Exit.
  "y0","z0",
]

assertArrayEquals(expected, log);
assertEquals(48, s);
assertNull(exception);
