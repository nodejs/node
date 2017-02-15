// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-async-await --expose-debug-as debug --allow-natives-syntax

// The test observes the callbacks that async/await makes to the inspector
// to make accurate stack traces. The pattern is based on saving a stack once
// with enqueueRecurring and restoring it multiple times.

// Additionally, the limited number of events is an indirect indication that
// we are not doing extra Promise processing that could be associated with memory
// leaks (v8:5380). In particular, no stacks are saved and restored for extra
// Promise handling on throwaway Promises.

// TODO(littledan): Write a test that demonstrates that the memory leak in
// the exception case is fixed.

Debug = debug.Debug;

var base_id = -1;
var exception = null;
var expected = [
  'enqueueRecurring #1',
  'willHandle #1',
  'then #1',
  'didHandle #1',
  'willHandle #1',
  'then #2',
  'cancel #1',
  'didHandle #1',
];

function assertLog(msg) {
  print(msg);
  assertTrue(expected.length > 0);
  assertEquals(expected.shift(), msg);
  if (!expected.length) {
    Debug.setListener(null);
  }
}

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.AsyncTaskEvent) return;
  try {
    if (base_id < 0)
      base_id = event_data.id();
    var id = event_data.id() - base_id + 1;
    assertTrue("async function" == event_data.name());
    assertLog(event_data.type() + " #" + id);
  } catch (e) {
    print(e + e.stack)
    exception = e;
  }
}

Debug.setListener(listener);

var resolver;
var p = new Promise(function(resolve, reject) {
  resolver = resolve;
});

async function main() {
  await p;
  assertLog("then #1");
  await undefined;
  assertLog("then #2");
}
main();
resolver();

%RunMicrotasks();

assertNull(exception);
