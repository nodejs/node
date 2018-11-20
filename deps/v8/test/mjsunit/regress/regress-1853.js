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

// Test whether scripts compiled after setting the break point are
// updated correctly.

Debug = debug.Debug;

var break_count = 0;
var test_break_1 = false;
var test_break_2 = false;

function sendCommand(state, cmd) {
  // Get the debug command processor in paused state.
  var dcp = state.debugCommandProcessor(false);
  var request = JSON.stringify(cmd);
  var response = dcp.processDebugJSONRequest(request);
  return JSON.parse(response);
}

function setBreakPointByName(state) {
  sendCommand(state, {
    seq: 0,
    type: "request",
    command: "setbreakpoint",
    arguments: {
      type: "script",
      target: "testScriptOne",
      line: 2
    }
  });
}

function setBreakPointByRegExp(state) {
  sendCommand(state, {
    seq: 0,
    type: "request",
    command: "setbreakpoint",
    arguments: {
      type: "scriptRegExp",
      target: "Scrip.Two",
      line: 2
    }
  });
}

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      switch (break_count) {
        case 0:
          // Set break points before the code has been compiled.
          setBreakPointByName(exec_state);
          setBreakPointByRegExp(exec_state);
          break;
        case 1:
          // Set the flag to prove that we hit the first break point.
          test_break_1 = true;
          break;
        case 2:
          // Set the flag to prove that we hit the second break point.
          test_break_2 = true;
          break;
      }
      break_count++;
    }
  } catch (e) {
    print(e);
  }
}

Debug.setListener(listener);
debugger;

eval('function test1() {                \n' +
     '  assertFalse(test_break_1);      \n' +
     '  assertTrue(test_break_1);       \n' +
     '}                                 \n' +
     '//# sourceURL=testScriptOne');

eval('function test2() {                \n' +
     '  assertFalse(test_break_2);      \n' +
     '  assertTrue(test_break_2);       \n' +
     '}                                 \n' +
     '//# sourceURL=testScriptTwo');

test1();
test2();
assertEquals(3, break_count);
