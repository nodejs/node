// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Debug = debug.Debug;

function assertEqualsAsync(expected, run, msg) {
  var actual;
  var hadValue = false;
  var hadError = false;
  var promise = run();

  if (typeof promise !== "object" || typeof promise.then !== "function") {
    throw new MjsUnitAssertionError(
        "Expected " + run.toString() +
        " to return a Promise, but it returned " + promise);
  }

  promise.then(function(value) { hadValue = true; actual = value; },
               function(error) { hadError = true; actual = error; });

  assertFalse(hadValue || hadError);

  %PerformMicrotaskCheckpoint();

  if (hadError) throw actual;

  assertTrue(
      hadValue, "Expected '" + run.toString() + "' to produce a value");

  assertEquals(expected, actual, msg);
}

var break_count = 0;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    break_count++;
    var line = exec_state.frame(0).sourceLineText();
    assertTrue(line.indexOf(`B${break_count}`) > 0);
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

async function g() {
  throw 1;
}

async function f() {
  try {
    await g();                   // B1
  } catch (e) {}
  assertEquals(2, break_count);  // B2
  return 1;                      // B3
}

Debug.setBreakPoint(f, 2);
Debug.setBreakPoint(f, 4);
Debug.setBreakPoint(f, 5);

f();

%PerformMicrotaskCheckpoint();

assertEqualsAsync(3, async () => break_count);
assertEqualsAsync(null, async () => exception);

Debug.setListener(null);
