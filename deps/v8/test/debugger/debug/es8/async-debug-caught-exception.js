// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

var exception = null;
var log;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Exception) return;
  try {
    var line = exec_state.frame(0).sourceLineText();
    var match = /Exception (\w)/.exec(line);
    assertNotNull(match);
    log.push(match[1]);
  } catch (e) {
    exception = e;
  }
}

async function thrower() {
  throw "a";  // Exception a
}

async function caught_throw() {
  try {
    await thrower();
  } catch (e) {
    assertEquals("a", e);
  }
}


// Caught throw, events on any exception.
log = [];
Debug.setListener(listener);
Debug.setBreakOnException();
caught_throw();
%RunMicrotasks();
Debug.setListener(null);
Debug.clearBreakOnException();
assertEquals(["a"], log);
assertNull(exception);

// Caught throw, events on uncaught exception.
log = [];
Debug.setListener(listener);
Debug.setBreakOnUncaughtException();
caught_throw();
%RunMicrotasks();
Debug.setListener(null);
Debug.clearBreakOnUncaughtException();
assertEquals([], log);
assertNull(exception);

var reject = Promise.reject("b");

async function caught_reject() {
  try {
    await reject;
  } catch (e) {
    assertEquals("b", e);
  }
}

// Caught reject, events on any exception.
log = [];
Debug.setListener(listener);
Debug.setBreakOnException();
caught_reject();
%RunMicrotasks();
Debug.setListener(null);
Debug.clearBreakOnException();
assertEquals([], log);
assertNull(exception);

// Caught reject, events on uncaught exception.
log = [];
Debug.setListener(listener);
Debug.setBreakOnUncaughtException();
caught_reject();
%RunMicrotasks();
Debug.setListener(null);
Debug.clearBreakOnUncaughtException();
assertEquals([], log);
assertNull(exception);

log = [];
Debug.setListener(listener);
Debug.setBreakOnException();

// "rethrown" uncaught exceptions in return don't cause another event
async function propagate_inner() { return thrower(); }
async function propagate_outer() { return propagate_inner(); }

propagate_outer();
%RunMicrotasks();
assertEquals(["a"], log);
assertNull(exception);

// Also don't propagate if an await interceded
log = [];
async function propagate_await() { await 1; return thrower(); }
async function propagate_await_outer() { return propagate_await(); }
propagate_await_outer();
%RunMicrotasks();
assertEquals(["a"], log);
assertNull(exception);

Debug.clearBreakOnException();
Debug.setBreakOnUncaughtException();

log = [];
Promise.resolve().then(() => Promise.reject()).catch(() => log.push("d")); // Exception c
%RunMicrotasks();
assertEquals(["d"], log);
assertNull(exception);

Debug.clearBreakOnUncaughtException();
Debug.setListener(null);

// If devtools is turned on in the middle, then catch prediction
// could be wrong (here, it mispredicts the exception as caught),
// but shouldn't crash.

log = [];

var resolve;
var turnOnListenerPromise = new Promise(r => resolve = r);
async function confused() {
  await turnOnListenerPromise;
  throw foo
}
confused();
Promise.resolve().then(() => {
  Debug.setListener(listener);
  Debug.setBreakOnUncaughtException();
  resolve();
});

assertEquals([], log);
