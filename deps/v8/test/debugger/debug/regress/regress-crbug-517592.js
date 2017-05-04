// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var source =
  "var foo = function foo() {\n" +
  "  return 1;\n" +
  "}\n" +
  "//@ sourceURL=test";

Debug = debug.Debug;
Debug.setListener(listener);
var exception = null;
var break_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) break_count++;
  if (event != Debug.DebugEvent.AfterCompile) return;
  try {
    var name = event_data.script().name();
    var id = event_data.script().id();
    assertEquals("test", name);
    Debug.setScriptBreakPointById(id, 2);
  } catch (e) {
    exception = e;
  }
}

eval(source);

assertEquals(0, break_count);
foo();
assertEquals(1, break_count);
assertNull(exception);
