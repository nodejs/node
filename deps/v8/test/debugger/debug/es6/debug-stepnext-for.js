// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony

Debug = debug.Debug;
var break_count = 0
var exception = null;
var log = []

var s = 0;
var a = [1, 2, 3];
var b = [1, 2, 3, 4];
var c = [Promise.resolve(1), Promise.resolve(2), Promise.resolve(3)];
var null_value = null;
var i = 0;

async function f() {
  "use strict";
  debugger;                      // Break a
  var j;                         // Break b

  for (var i in null_value) {    // Break c
    s += a[i];
  }

  for (j in null_value) {        // Break d
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

  for (let i  of  a) {           // Break j
    s += i;                      // Break J
  }

  for (var i = 0; i < 3; i++) {  // Break k
    s += a[i];                   // Break K
  }

  for (j = 0; j < 3; j++) {      // Break l
    s += a[j];                   // Break L
  }

  for (let i = 0; i < 3; i++) {  // Break m
    s += a[i];                   // Break M
  }

  for (let i of a) {}            // Break n

  [1, ...a]                      // Break o

  for await (let i of c) {       // Break p
    s += i;                      // Break P
  }

  for await (j of c) {           // Break q
    s += j;                      // Break Q
  }

}                                // Break y

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    var col = exec_state.frame(0).sourceColumn();
    var lineWithBreakMarker = line.substring(0, col) + '#' + line.substring(col);
    print(lineWithBreakMarker);
    var match = line.match(/\/\/ Break (\w)$/);
    assertEquals(2, match.length);
    log.push(match[1] + col);
    exec_state.prepareStep(Debug.StepAction.StepOver);
    break_count++;
  } catch (e) {
    exception = e;
  }
}

(async () => {
  Debug.setListener(listener);
  await f();
  Debug.setListener(null);         // Break z

  print("log:\n" + JSON.stringify(log));
  // The let declaration differs from var in that the loop variable
  // is declared in every iteration.
  // TODO(verwaest): For-of has hacky position numbers for Symbol.iterator and
  // .next. Restore to proper positions once the CallPrinter can disambiguate
  // based on other values.
  var expected = [
    // Entry
    'a2',
    // Empty for-in-var: get enumerable
    'c16',
    // Empty for-in: get enumerable
    'd12',
    // For-in-var: get enumerable, assign, body, assign, body, ...
    'e16',
    'e11',
    'E4',
    'e11',
    'E4',
    'e11',
    'E4',
    // For-in: get enumerable, assign, body, assign, body, ...
    'f12',
    'f7',
    'F4',
    'f7',
    'F4',
    'f7',
    'F4',
    // For-in-let: get enumerable, assign, body, assign,  ...
    'g16',
    'g11',
    'G4',
    'g11',
    'G4',
    'g11',
    'G4',
    // For-of-var: [Symbol.iterator](), assign, body, assign, body, ...
    'h16',
    'h11',
    'H4',
    'h11',
    'H4',
    'h11',
    'H4',
    // For-of: [Symbol.iterator](), assign, body, assign, body, ...
    'i12',
    'i7',
    'I4',
    'i7',
    'I4',
    'i7',
    'I4',
    // For-of-let: [Symbol.iterator](), assign, body, assign, ...
    'j18',
    'j11',
    'J4',
    'j11',
    'J4',
    'j11',
    'J4',
    // For-var: init, condition, body, next, condition, body, ...
    'k15',
    'k20',
    'K4',
    'k26',
    'k20',
    'K4',
    'k26',
    'k20',
    'K4',
    'k26',
    'k20',
    // For: init, condition, body, next, condition, body, ...
    'l7',
    'l16',
    'L4',
    'l22',
    'l16',
    'L4',
    'l22',
    'l16',
    'L4',
    'l22',
    'l16',
    // For-let: init, condition, body, next, condition, body, ...
    'm15',
    'm20',
    'M4',
    'm26',
    'm20',
    'M4',
    'm26',
    'm20',
    'M4',
    'm26',
    'm20',
    // For-of, empty: [Symbol.iterator](), assign once
    'n16',
    'n11',
    // Spread: expression statement
    'o2',
    // For-await-of: [Symbol.iterator](), assign, body, assign, body, ...
    'p22',
    'p17',
    'P4',
    'p17',
    'P4',
    'p17',
    'P4',
    // For-await-of-let: [Symbol.iterator](), assign, body, assign, body, ...
    'q18',
    'q13',
    'Q4',
    'q13',
    'Q4',
    'q13',
    'Q4',
    // Exit.
    'y0',
    'z2',
  ]
  print("expected:\n" + JSON.stringify(expected));

  assertArrayEquals(expected, log);
  assertEquals(66, s);
  assertNull(exception);

})();
