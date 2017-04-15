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

Debug = debug.Debug

// Simple function which stores the last debug event.
var listenerComplete = false;
var exception = false;
var f_script_id = 0;
var g_script_id = 0;
var h_script_id = 0;
var f_line = 0;
var g_line = 0;
var h_line = 0;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      if (listenerComplete) return;

      Debug.setScriptBreakPointById(f_script_id, f_line);
      Debug.setScriptBreakPointById(g_script_id, g_line);
      Debug.setScriptBreakPointById(h_script_id, h_line);

      // Indicate that all was processed.
      listenerComplete = true;
    }
  } catch (e) {
    exception = e
    print(e)
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function f() {
  a=1;
};

function g() {
  // Comment.
  f();
};

eval('function h(){}');
eval('function sourceUrlFunc() { a = 2; }\n//# sourceURL=sourceUrlScript');

o = {a:function(){},b:function(){}}

// Check the script ids for the test functions.
f_script_id = Debug.findScript(f).id;
g_script_id = Debug.findScript(g).id;
h_script_id = Debug.findScript(h).id;
sourceURL_script_id = Debug.findScript(sourceUrlFunc).id;

assertTrue(f_script_id > 0, "invalid script id for f");
assertTrue(g_script_id > 0, "invalid script id for g");
assertTrue(h_script_id > 0, "invalid script id for h");
assertTrue(sourceURL_script_id > 0, "invalid script id for sourceUrlFunc");
assertEquals(f_script_id, g_script_id);

// Get the source line for the test functions.
f_line = Debug.findFunctionSourceLocation(f).line;
g_line = Debug.findFunctionSourceLocation(g).line;
h_line = Debug.findFunctionSourceLocation(h).line;
assertTrue(f_line > 0, "invalid line for f");
assertTrue(g_line > 0, "invalid line for g");
assertTrue(f_line < g_line);
assertEquals(h_line, 0, "invalid line for h");

// Set a break point and call to invoke the debug event listener.
Debug.setBreakPoint(g, 0, 0);
g();

// Make sure that the debug event listener was invoked.
assertTrue(listenerComplete, "listener did not run to completion: " + exception);

// Try setting breakpoint by url specified in sourceURL

var breakListenerCalled = false;

function breakListener(event) {
  if (event == Debug.DebugEvent.Break)
    breakListenerCalled = true;
}

Debug.setBreakPoint(sourceUrlFunc);

Debug.setListener(breakListener);

sourceUrlFunc();

assertTrue(breakListenerCalled, "Break listener not called on breakpoint set by sourceURL");


// Breakpoint in a script with no statements test case. If breakpoint is set
// to the script body, its actual position is taken from the nearest statement
// below or like in this case is reset to the very end of the script.
// Unless some precautions made, this position becomes out-of-range and
// we get an exception.

// Gets a script of 'i1' function and sets the breakpoint at line #4 which
// should be empty.
function SetBreakpointInI1Script() {
  var i_script = Debug.findScript(i1);
  assertTrue(!!i_script, "invalid script for i1");
  Debug.setScriptBreakPoint(Debug.ScriptBreakPointType.ScriptId,
                            i_script.id, 4);
}

// Creates the eval script and tries to set the breakpoint.
// The tricky part is that the script function must be strongly reachable at the
// moment. Since there's no way of simply getting the pointer to the function,
// we run this code while the script function is being activated on stack.
eval('SetBreakpointInI1Script()\nfunction i1(){}\n\n\n\nfunction i2(){}\n');
