// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

Debug = debug.Debug;

var base_id = -1;
var exception = null;
var expected = [
  "enqueue #1",
  "willHandle #1",
  "didHandle #1",
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
    assertEquals("Object.observe", event_data.name());
    assertLog(event_data.type() + " #" + id);
  } catch (e) {
    print(e + e.stack)
    exception = e;
  }
}

Debug.setListener(listener);

var obj = {};
Object.observe(obj, function(changes) {
  print(change.type + " " + change.name + " " + change.oldValue);
});

obj.foo = 1;
obj.zoo = 2;
obj.foo = 3;

assertNull(exception);
