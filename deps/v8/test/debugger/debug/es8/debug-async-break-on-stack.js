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

  %RunMicrotasks();

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
    print(line);
    assertTrue(line.indexOf(`B${break_count}`) > 0);
  } catch (e) {
    exception = e;
  }
}


async function g() {
  setbreaks();
  throw 1;  // B1
}

async function f() {
  try {
    await g();
  } catch (e) {}
  return 2;  // B2
}

function setbreaks() {
  Debug.setListener(listener);
  Debug.setBreakPoint(g, 2);
  Debug.setBreakPoint(f, 4);
}

f();

%RunMicrotasks();

assertEqualsAsync(2, async () => break_count);
assertEqualsAsync(null, async () => exception);

Debug.setListener(null);
