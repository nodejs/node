// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


Debug = debug.Debug
var exception = null;
var yields = 0;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var source = exec_state.frame(0).sourceLineText();
    print(source);
    if (/stop stepping/.test(source)) return;
    if (/yield/.test(source)) yields++;
    if (yields == 4) {
      exec_state.prepareStep(Debug.StepAction.StepOut);
    } else {
      exec_state.prepareStep(Debug.StepAction.StepInto);
    }
  } catch (e) {
    print(e, e.stack);
    exception = e;
  }
};

Debug.setListener(listener);

function* g() {
  for (var i = 0; i < 3; ++i) {
    yield i;
  }
}

var i = g();
debugger;
for (var num of g()) {}
i.next();

print(); // stop stepping

// Not stepped into.
i.next();
i.next();

assertNull(exception);
assertEquals(4, yields);
