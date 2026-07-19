// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Debug = debug.Debug

function foo(){}

let breakpoint_count = 0;
let last_source_line = 0;
let last_source_column = 0;
function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    ++breakpoint_count;
    last_source_line = exec_state.frame(0).sourceLine();
    last_source_column = exec_state.frame(0).sourceColumn();
  }
};

Debug.setListener(listener);

// Run without breakpoints.
foo();
assertEquals(breakpoint_count, 0);

// Run with breakpoint.
const breakpoint = Debug.setBreakPoint(foo, 0);
foo();
assertEquals(breakpoint_count, 1);
assertEquals(last_source_line, 7);
assertEquals(last_source_column, 15);
foo();
assertEquals(breakpoint_count, 2);
assertEquals(last_source_line, 7);
assertEquals(last_source_column, 15);

// Run without breakpoints
Debug.clearBreakPoint(breakpoint);
foo();
assertEquals(breakpoint_count, 2);
