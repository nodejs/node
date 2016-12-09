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

// The functions used for testing backtraces.
function Point(x, y) {
  this.x = x;
  this.y = y;
};

Point.prototype.distanceTo = function(p) {
  debugger;
  return Math.sqrt(Math.pow(Math.abs(this.x - p.x), 2) +
         Math.pow(Math.abs(this.y - p.y), 2))
}

p1 = new Point(1,1);
p2 = new Point(2,2);

p1.distanceTo = function(p) {
  return p.distanceTo(this);
}

function distance(p, q) {
  return p.distanceTo(q);
}

function createPoint(x, y) {
  return new Point(x, y);
}

a=[1,2,distance];

// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

what = 'constructor';  // Flag to control which part of the test is run.
listenerCalled = false;
exception = false;

function safeEval(code) {
  try {
    return eval('(' + code + ')');
  } catch (e) {
    return undefined;
  }
}

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      if (what == 'constructor') {
        // The expected backtrace is
        // 0: Call distance on Point where distance is a prototype property
        // 1: Call distance on Point where distance is a direct property
        // 2: Call on function an array element 2
        // 3: [anonymous]
        assertEquals("#<Point>.distanceTo(p=#<Point>)",
                     exec_state.frame(0).invocationText());
        assertEquals("#<Point>.distanceTo(p=#<Point>)",
                     exec_state.frame(1).invocationText());
        assertEquals("#<Array>[2](aka distance)(p=#<Point>, q=#<Point>)",
                     exec_state.frame(2).invocationText());
        assertEquals("[anonymous]()", exec_state.frame(3).invocationText());
        listenerCalled = true;
      } else if (what == 'breakpoint') {
        // The expected backtrace is
        // 0: Call Point constructor
        // 1: Call on global function createPoint
        // 2: [anonymous]
        assertEquals("new Point(x=0, y=0)",
                     exec_state.frame(0).invocationText());
        assertEquals("createPoint(x=0, y=0)",
                     exec_state.frame(1).invocationText());
        assertEquals("[anonymous]()", exec_state.frame(2).invocationText());
        listenerCalled = true;
      } else if (what == 'symbol') {
        // The expected backtrace is
        // 0: Call Point constructor
        // 1: Call on symbol method
        // 2: [anonymous]
        assertEquals("new Point(x=0, y=0)",
                     exec_state.frame(0).invocationText());
        assertEquals("#<Object>[Symbol(Das Symbol)](x=0, y=0)",
                     exec_state.frame(1).invocationText());
        assertEquals("[anonymous]()", exec_state.frame(2).invocationText());
        listenerCalled = true;
      } else {
        assertUnreachable();
      }
    }
  } catch (e) {
    exception = e
  };
};

// Add the debug event listener.
Debug.setListener(listener);

// Set a break point and call to invoke the debug event listener.
a[2](p1, p2)

// Make sure that the debug event listener vas invoked.
assertTrue(listenerCalled);
assertFalse(exception, "exception in listener")

// Set a break point and call to invoke the debug event listener.
what = 'breakpoint';
listenerCalled = false;
Debug.setBreakPoint(Point, 0, 0);
createPoint(0, 0);

// Make sure that the debug event listener vas invoked (again).
assertTrue(listenerCalled);
assertFalse(exception, "exception in listener")

what = 'symbol';
listenerCalled = false;
var S = Symbol('Das Symbol');
var o = { [S](x, y) { return new Point(x, y); } };
Debug.setBreakPoint(Point, 0, 0);
o[S](0, 0);

assertTrue(listenerCalled);
assertFalse(exception, "exception in listener")
