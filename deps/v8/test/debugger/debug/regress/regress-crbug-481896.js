// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --expose-gc
gc();
function static() {
  print("> static");  // Break
}

var Debug = debug.Debug;
var exception = null;
var break_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    print("breakpoint hit at " + exec_state.frame(0).sourceLineText());
    assertTrue(exec_state.frame(0).sourceLineText().indexOf("// Break") > 0);
    break_count++;
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

function install() {
  eval("this.dynamic = function dynamic() { \n" +
       "  print(\"> dynamic\");  // Break\n" +
       "}\n" +
       "//@ sourceURL=dynamicScript");
}

install();

var scripts = Debug.scripts();
var dynamic_script;
var static_script;
for (var script of scripts) {
  if (script.source_url == "dynamicScript") dynamic_script = script;
  if (script.source_url == "staticScript") static_script = script;
}

Debug.setScriptBreakPointById(dynamic_script.id, 1);
Debug.setScriptBreakPointById(static_script.id, 6);

dynamic();
static();

Debug.setListener(null);

assertNull(exception);
assertEquals(2, break_count);

//@ sourceURL=staticScript
