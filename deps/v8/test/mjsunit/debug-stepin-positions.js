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

// Flags: --expose-debug-as debug --nocrankshaft
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

function DebuggerStatement() {
  debugger;  /*pause*/
}

function TestCase(fun, frame_number) {
  var exception = false;
  var codeSnippet = undefined;
  var resultPositions = undefined;

  function listener(event, exec_state, event_data, data) {
    try {
      if (event == Debug.DebugEvent.Break ||
          event == Debug.DebugEvent.Exception) {
        Debug.setListener(null);
        assertHasLineMark(/pause/, exec_state.frame(0));
        assertHasLineMark(/positions/, exec_state.frame(frame_number));
        var frame = exec_state.frame(frame_number);
        codeSnippet = frame.sourceLineText();
        resultPositions = frame.stepInPositions();
      }
    } catch (e) {
      exception = e
    }

    function assertHasLineMark(mark, frame) {
        var line = frame.sourceLineText();
        if (!mark.exec(frame.sourceLineText())) {
            throw new Error("Line " + line + " should contain mark " + mark);
        }
    }
  }

  Debug.setListener(listener);

  fun();

  Debug.setListener(null);

  assertTrue(!exception, exception);

  var expectedPositions = {};
  var markPattern = new RegExp("/\\*#\\*/", "g");

  var matchResult;
  while ( (matchResult = markPattern.exec(codeSnippet)) ) {
    expectedPositions[matchResult.index] = true;
  }

  print(codeSnippet);

  var decoratedResult = codeSnippet;

  function replaceStringRange(s, pos, substitute) {
   return s.substring(0, pos) + substitute +
       s.substring(pos + substitute.length);
  }

  var markLength = 5;
  var unexpectedPositionFound = false;

  for (var i = 0; i < resultPositions.length; i++) {
    var col = resultPositions[i].position.column - markLength;
    if (expectedPositions[col]) {
      delete expectedPositions[col];
      decoratedResult = replaceStringRange(decoratedResult, col, "*YES*");
    } else {
      decoratedResult = replaceStringRange(decoratedResult, col, "!BAD!");
      unexpectedPositionFound = true;
    }
  }

  print(decoratedResult);

  for (var n in expectedPositions) {
    assertTrue(false, "Some positions are not reported: " + decoratedResult);
    break;
  }
  assertFalse(unexpectedPositionFound, "Found unexpected position: " +
      decoratedResult);
}

function TestCaseWithDebugger(fun) {
  TestCase(fun, 1);
}

function TestCaseWithBreakpoint(fun, line_number, frame_number) {
  var breakpointId = Debug.setBreakPoint(fun, line_number);
  TestCase(fun, frame_number);
  Debug.clearBreakPoint(breakpointId);
}

function TestCaseWithException(fun, frame_number) {
  Debug.setBreakOnException();
  TestCase(fun, frame_number);
  Debug.clearBreakOnException();
}


// Test cases.

// Step in position, when the function call that we are standing at is already
// being executed.
var fun = function() {
  function g(p) {
    throw String(p); /*pause*/
  }
  try {
    var res = [ g(1), /*#*/g(2) ]; /*positions*/
  } catch (e) {
  }
};
TestCaseWithBreakpoint(fun, 2, 1);
TestCaseWithException(fun, 1);


// Step in position, when the function call that we are standing at is raising
// an exception.
var fun = function() {
  var o = {
    g: function(p) {
      throw p;
    }
  };
  try {
    var res = [ /*#*/f(1), /*#*/g(2) ]; /*pause, positions*/
  } catch (e) {
  }
};
TestCaseWithException(fun, 0);


// Step-in position, when already paused almost on the first call site.
var fun = function() {
  function g(p) {
    throw p;
  }
  try {
    var res = [ /*#*/g(Math.rand), /*#*/g(2) ]; /*pause, positions*/
  } catch (e) {
  }
};
TestCaseWithBreakpoint(fun, 5, 0);

// Step-in position, when already paused on the first call site.
var fun = function() {
  function g() {
    throw "Debug";
  }
  try {
    var res = [ /*#*/g(), /*#*/g() ]; /*pause, positions*/
  } catch (e) {
  }
};
TestCaseWithBreakpoint(fun, 5, 0);


// Method calls.
var fun = function() {
  var data = {
    a: function() {}
  };
  var res = [ DebuggerStatement(), data./*#*/a(), data[/*#*/String("a")]/*#*/(), data["a"]/*#*/(), data.a, data["a"] ]; /*positions*/
};
TestCaseWithDebugger(fun);

// Function call on a value.
var fun = function() {
  function g(p) {
      return g;
  }
  var res = [ DebuggerStatement(), /*#*/g(2), /*#*/g(2)/*#*/(3), /*#*/g(0)/*#*/(0)/*#*/(g) ]; /*positions*/
};
TestCaseWithDebugger(fun);

// Local function call, closure function call,
// local function construction call.
var fun = (function(p) {
  return function() {
    function f(a, b) {
    }
    var res = /*#*/f(DebuggerStatement(), /*#*/p(/*#*/new f())); /*positions*/
  };
})(Object);
TestCaseWithDebugger(fun);

// Global function, global object construction, calls before pause point.
var fun = (function(p) {
  return function() {
    var res = [ Math.abs(new Object()), DebuggerStatement(), Math./*#*/abs(4), /*#*/new Object()./*#*/toString() ]; /*positions*/
  };
})(Object);
TestCaseWithDebugger(fun);
