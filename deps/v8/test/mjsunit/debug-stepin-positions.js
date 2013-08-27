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
  debugger;
}

function TestCase(fun) {
  var exception = false;
  var codeSnippet = undefined;
  var resultPositions = undefined;

  function listener(event, exec_state, event_data, data) {
    try {
      if (event == Debug.DebugEvent.Break) {
        Debug.setListener(null);

        var secondFrame = exec_state.frame(1);
        codeSnippet = secondFrame.sourceLineText();
        resultPositions = secondFrame.stepInPositions();
      }
    } catch (e) {
      exception = e
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


// Test cases.

// Method calls.
var fun = function() {
  var data = {
    a: function() {}
  };
  var res = [ DebuggerStatement(), data./*#*/a(), data[/*#*/String("a")]/*#*/(), data["a"]/*#*/(), data.a, data["a"] ];
};
TestCase(fun);

// Function call on a value.
var fun = function() {
  function g(p) {
      return g;
  }
  var res = [ DebuggerStatement(), /*#*/g(2), /*#*/g(2)/*#*/(3), /*#*/g(0)/*#*/(0)/*#*/(g) ];
};
TestCase(fun);

// Local function call, closure function call,
// local function construction call.
var fun = (function(p) {
  return function() {
    function f(a, b) {
    }
    var res = /*#*/f(DebuggerStatement(), /*#*/p(/*#*/new f()));
  };
})(Object);
TestCase(fun);

// Global function, global object construction, calls before pause point.
var fun = (function(p) {
  return function() {
    var res = [ Math.abs(new Object()), DebuggerStatement(), Math./*#*/abs(4), /*#*/new Object()./*#*/toString() ];
  };
})(Object);
TestCase(fun);
