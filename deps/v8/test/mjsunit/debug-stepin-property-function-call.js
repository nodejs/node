// Copyright 2014 the V8 project authors. All rights reserved.
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

// Flags: --expose-debug-as debug --nocrankshaft
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

var exception = null;
var state = 1;

// Simple debug event handler which first time will cause 'step in' action
// to get into g.call and than check that execution is stopped inside
// function 'g'.
function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      if (state == 1) {
        exec_state.prepareStep(Debug.StepAction.StepIn, 3);
        state = 2;
      } else if (state == 2) {
        assertTrue(event_data.sourceLineText().indexOf("Expected to step") > 0,
          "source line: \"" + event_data.sourceLineText() + "\"");
        state = 3;
      }
    }
  } catch(e) {
    print("Exception: " + e);
    exception = e;
  }
};

// Add the debug event listener.
Debug.setListener(listener);

var count = 0;
var obj = {
  fun: function() {
    ++count;
    return count; // Expected to step
  }
};
obj.fun2 = obj.fun;

function testCall_Dots() {
  debugger;
  obj.fun();
}

function testCall_Quotes() {
  debugger;
  obj["fun"]();
}

function testCall_Call() {
  debugger;
  obj.fun.call(obj);
}

function testCall_Apply() {
  debugger;
  obj.fun.apply(obj);
}

function testCall_Variable() {
  var functionName = "fun";
  debugger;
  obj[functionName]();
}

function testCall_Fun2() {
  debugger;
  obj.fun2();
}

function testCall_InternStrings() {
  var cache = { "fun": "fun" };
  var functionName = "fu" + "n";
  debugger;
  obj[cache[functionName]]();
}

function testCall_ViaFunRef() {
  var functionName = "fu" + "n";
  var funRef = obj[functionName];
  debugger;
  funRef();
}

// bug 2888
function testCall_RuntimeVariable1() {
  var functionName = "fu" + "n";
  debugger;
  obj[functionName]();
}

// bug 2888
function testCall_RuntimeVariable2() {
  var functionName = "un".replace(/u/, "fu");
  debugger;
  obj[functionName]();
}

// bug 2888
function testCall_RuntimeVariable3() {
  var expr = "fu" + "n";
  const functionName = expr;
  assertEquals("fun", functionName);
  debugger;
  obj[functionName]();
}

var functionsCalled = 0;
for (var n in this) {
  if (n.substr(0, 4) != 'test' || typeof this[n] !== "function") {
    continue;
  }
  state = 1;
  print("Running " + n + "...");
  this[n]();
  ++functionsCalled;
  assertNull(exception, n);
  assertEquals(3, state, n);
  assertEquals(functionsCalled, count, n);
}

assertEquals(11, functionsCalled);

// Get rid of the debug event listener.
Debug.setListener(null);
