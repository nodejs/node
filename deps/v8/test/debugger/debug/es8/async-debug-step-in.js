// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Debug = debug.Debug;
var step_count = 0;

function listener(event, execState, eventData, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var line = execState.frame(0).sourceLineText();
    print(line);
    var [match, expected_count, step] = /\/\/ B(\d) (\w+)$/.exec(line);
    assertEquals(step_count++, parseInt(expected_count));
    if (step != "Continue") execState.prepareStep(Debug.StepAction[step]);
  } catch (e) {
    print(e, e.stack);
    quit(1);
  }
}

Debug.setListener(listener);

var late_resolve;

function g() {
  return new Promise(  // B2 StepIn
    function(res, rej) {
      late_resolve = res;  // B3 StepIn
    }                      // B4 StepIn
  );                       // B5 StepIn
}

async function f() {
  var a = 1;
  debugger;            // B0 StepNext
  a +=
       await           // B1 StepIn
             g();
  return a;            // B6 Continue
}

f().then(value => assertEquals(4, value));

late_resolve(3);

%PerformMicrotaskCheckpoint();

assertEquals(7, step_count);
