// Copyright 2013 the V8 project authors. All rights reserved.
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

Debug = debug.Debug

var error = null;
var test = 0;

function check_v(expected, exec_state, frame_id) {
  assertEquals(expected, exec_state.frame(frame_id).evaluate('v').value());
}

function listener(event, exec_state, event_data, data) {
  try {
    if (event != Debug.DebugEvent.Break) return;
    test++;
    if (test == 1) {
      check_v('inner0', exec_state, 0);
      check_v('inner0', exec_state, 1);
      check_v('outer',  exec_state, 2);
      assertArrayEquals(["a", "b", "c"],
                        exec_state.frame(0).evaluate('arguments').value());
    } else if (test == 2) {
      check_v('inner1', exec_state, 0);
      check_v('inner1', exec_state, 1);
      check_v('outer',  exec_state, 2);
      assertArrayEquals(["a", "b", "c"],
                        exec_state.frame(0).evaluate('arguments').value());
    } else {
      assertEquals(3, test);
      check_v('inner2', exec_state, 0);
      check_v('inner1', exec_state, 1);
      check_v('inner1', exec_state, 2);
      check_v('outer',  exec_state, 3);
      assertArrayEquals(["x", "y", "z"],
                        exec_state.frame(0).evaluate('arguments').value());
      assertArrayEquals(["a", "b", "c"],
                        exec_state.frame(1).evaluate('arguments').value());
    }
  } catch (e) {
    error = e;
  }
};

Debug.setListener(listener);

var v = 'outer';
(function() {  // Test 1 and 2
  var v = 'inner0';
  eval("debugger; var v = 'inner1'; debugger;");
  assertEquals('inner1', v);  // Overwritten by local eval.
})("a", "b", "c");
assertNull(error);

(function() {  // Test 3
  var v = 'inner0';  // Local eval overwrites this value.
  eval("var v = 'inner1'; " +
       "(function() { var v = 'inner2'; debugger; })('x', 'y', 'z');");
  assertEquals('inner1', v);  // Overwritten by local eval.
})("a", "b", "c");
assertNull(error);
