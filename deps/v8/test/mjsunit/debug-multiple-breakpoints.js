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

// Test functions
function f() {a=1;b=2;};
function g() {f();}
function h() {}

// This test sets several break points at the same place and checks that
// several break points at the same place only makes one debug break event
// and that when the last break point is removed no more debug break events
// occours.
break_point_hit_count = 0;

// Set a breakpoint in f.
bp1 = Debug.setBreakPoint(f);
f();
assertEquals(1, break_point_hit_count);

// Set another breakpoint in f at the same place.
bp2 = Debug.setBreakPoint(f);
f();
assertEquals(2, break_point_hit_count);

// Remove one of the break points.
Debug.clearBreakPoint(bp1);
f();
assertEquals(3, break_point_hit_count);

// Remove the second break point.
Debug.clearBreakPoint(bp2);
f();
assertEquals(3, break_point_hit_count);

// Perform the same test using function g (this time removing the break points
// in the another order).
break_point_hit_count = 0;
bp1 = Debug.setBreakPoint(g);
g();
assertEquals(1, break_point_hit_count);
bp2 = Debug.setBreakPoint(g);
g();
assertEquals(2, break_point_hit_count);
Debug.clearBreakPoint(bp2);
g();
assertEquals(3, break_point_hit_count);
Debug.clearBreakPoint(bp1);
g();
assertEquals(3, break_point_hit_count);

// Finally test with many break points.
test_count = 100;
bps = new Array(test_count);
break_point_hit_count = 0;
for (var i = 0; i < test_count; i++) {
  bps[i] = Debug.setBreakPoint(h);
  h();
}
for (var i = 0; i < test_count; i++) {
  h();
  Debug.clearBreakPoint(bps[i]);
}
assertEquals(test_count * 2, break_point_hit_count);
h();
assertEquals(test_count * 2, break_point_hit_count);
