// Copyright 2012 the V8 project authors. All rights reserved.
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

function FindCallFrame(exec_state, frame_code) {
  var number = Number(frame_code);
  if (number >= 0) {
    return exec_state.frame(number);
  } else {
    for (var i = 0; i < exec_state.frameCount(); i++) {
      var frame = exec_state.frame(i);
      var func_mirror = frame.func();
      if (frame_code == func_mirror.name()) {
        return frame;
      }
    }
  }
  throw new Error("Failed to find function name " + function_name);
}

function TestCase(test_scenario, expected_output) {
  // Global variable, accessed from eval'd script.
  test_output = "";

  function TestCode() {
    function A() {
      // Extra stack variable. To make function not slim.
      // Restarter doesn't work on slim function when stopped on 'debugger'
      // statement. (There is no padding for 'debugger' statement).
      var o = {};
      test_output += 'A';
      test_output += '=';
      debugger;
      return 'Capybara';
    }
    function B(p1, p2) {
      test_output += 'B';
      return A();
    }
    function C() {
      test_output += 'C';
      // Function call with argument adaptor is intentional.
      return B();
    }
    function D() {
      test_output += 'D';
      // Function call with argument adaptor is intentional.
      return C(1, 2);
    }
    function E() {
      test_output += 'E';
      return D();
    }
    function F() {
      test_output += 'F';
      return E();
    }
    return F();
  }

  var scenario_pos = 0;

  function DebuggerStatementHandler(exec_state) {
    while (true) {
      assertTrue(scenario_pos < test_scenario.length);
      var change_code = test_scenario[scenario_pos++];
      if (change_code == '=') {
        // Continue.
        return;
      }
      var frame = FindCallFrame(exec_state, change_code);
      // Throws if fails.
      Debug.LiveEdit.RestartFrame(frame);
    }
  }

  var saved_exception = null;

  function listener(event, exec_state, event_data, data) {
    if (saved_exception != null) {
      return;
    }
    if (event == Debug.DebugEvent.Break) {
      try {
        DebuggerStatementHandler(exec_state);
      } catch (e) {
        saved_exception = e;
      }
    } else {
      print("Other: " + event);
    }
  }

  Debug.setListener(listener);
  assertEquals("Capybara", TestCode());
  Debug.setListener(null);

  if (saved_exception) {
    print("Exception: " + saved_exception);
    print("Stack: " + saved_exception.stack);
    assertUnreachable();
  }

  print(test_output);

  assertEquals(expected_output, test_output);
}

TestCase('0==', "FEDCBA=A=");
TestCase('1==', "FEDCBA=BA=");
TestCase('2==', "FEDCBA=CBA=");
TestCase('3==', "FEDCBA=DCBA=");
TestCase('4==', "FEDCBA=EDCBA=");
TestCase('5==', "FEDCBA=FEDCBA=");

TestCase('=', "FEDCBA=");

TestCase('C==', "FEDCBA=CBA=");

TestCase('B=C=A=D==', "FEDCBA=BA=CBA=A=DCBA=");

// Successive restarts don't work now and require additional fix.
//TestCase('BCDE==', "FEDCBA=EDCBA=");
//TestCase('BC=BCDE==', "FEDCBA=CBA=EDCBA=");
//TestCase('EF==', "FEDCBA=FEDCBA=");
