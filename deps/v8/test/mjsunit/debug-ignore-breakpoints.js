// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --expose-debug-as debug
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

// Simple debug event handler which just counts the number of break points hit.
var break_point_hit_count;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    break_point_hit_count++;
  }
};

// Add the debug event listener.
Debug.setListener(listener);

// Test function.
function f() {};

// This tests ignore of break points including the case with several
// break points in the same location.
break_point_hit_count = 0;

// Set a breakpoint in f.
bp1 = Debug.setBreakPoint(f);

// Try ignore count of 1.
Debug.changeBreakPointIgnoreCount(bp1, 1);
f();
assertEquals(0, break_point_hit_count);
f();
assertEquals(1, break_point_hit_count);

// Set another breakpoint in f at the same place.
bp2 = Debug.setBreakPoint(f);
f();
assertEquals(2, break_point_hit_count);

// Set different ignore counts.
Debug.changeBreakPointIgnoreCount(bp1, 2);
Debug.changeBreakPointIgnoreCount(bp2, 4);
f();
assertEquals(2, break_point_hit_count);
f();
assertEquals(2, break_point_hit_count);
f();
assertEquals(3, break_point_hit_count);
f();
assertEquals(4, break_point_hit_count);

// Set different ignore counts (opposite).
Debug.changeBreakPointIgnoreCount(bp1, 4);
Debug.changeBreakPointIgnoreCount(bp2, 2);
f();
assertEquals(4, break_point_hit_count);
f();
assertEquals(4, break_point_hit_count);
f();
assertEquals(5, break_point_hit_count);
f();
assertEquals(6, break_point_hit_count);

