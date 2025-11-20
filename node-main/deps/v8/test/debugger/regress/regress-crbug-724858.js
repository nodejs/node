// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Debug = debug.Debug;
let exception = null;
let step_count = 0;

Debug.setListener((event, execState, eventData, data) => {
  if (event != Debug.DebugEvent.Break) return;
  try {
    const line = execState.frame(0).sourceLineText();
    print(line);

    const [match, expected_count, step] = /\/\/ B(\d) (\w+)$/.exec(line);
    assertEquals(step_count++, parseInt(expected_count));

    if (step != "Continue") execState.prepareStep(Debug.StepAction[step]);
  } catch (e) {
    print(e, e.stack);
    exception = e;
  }
});

function f(x) {
  debugger;          // B0 StepOver
  with ({}) {        // B1 StepOver
    return x         // B2 StepOver
    ;                // B3 Continue
  }
}

f(42);

assertNull(exception);
assertEquals(4, step_count);
