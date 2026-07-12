// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Debug = debug.Debug;

let exception = null;
function listener(event, execState, eventData, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    execState.frame().evaluate("var a = 1").value();
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);

function foo(arg) {
  return function() {
    // Context allocate arg.
    arg;
    debugger;
  }
}
foo()();

assertNotNull(exception);
assertContains(
  "Identifier 'a' cannot be declared with 'var' in current evaluation scope",
  exception.message);
