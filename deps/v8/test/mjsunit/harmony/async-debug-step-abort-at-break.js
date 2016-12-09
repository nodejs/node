// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --allow-natives-syntax --harmony-async-await

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
  return new Promise( // B3 StepOut
    function(res, rej) {
      late_resolve = res;
    }
  );
}

async function f() {
  var a = 1;
  debugger;          // B0 StepNext
  a +=               // B1 StepNext
       await         // B4 StepNext
             g();    // B2 StepIn
  return a;
}

f();

// Starting a new step action at an intermediate break point
// means that we will abort the current async step.
debugger;            // B5 StepNext

late_resolve(3);     // B6 Continue

%RunMicrotasks();

assertEquals(7, step_count);
