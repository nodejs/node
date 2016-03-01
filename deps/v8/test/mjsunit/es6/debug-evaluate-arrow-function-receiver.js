// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

// Test that debug-evaluate can find the correct this value for an arrow
// function, if "this" is referenced within the arrow function scope.

Debug = debug.Debug

var break_count = 0;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    for (var i = 0; i < exec_state.frameCount() - 1; i++) {
      var frame = exec_state.frame(i);
      var this_value = frame.evaluate("this").value();
      var expected = frame.sourceLineText().match(/\/\/ (.*$)/)[1];
      print(expected, this_value, frame.sourceLineText());
      assertEquals(String(expected), String(this_value));
    }
    break_count++;
  } catch (e) {
    exception = e;
    print(e + e.stack);
  }
}

// Context-allocated receiver.
function f() {
  debugger;                          // foo
  return () => {
    debugger;                        // foo
    with ({}) {
      return () => {
        debugger;                    // foo
        try {
          throw new Error();
        } catch (e) {
          return () => {
            (() => this);            // bind this.
            debugger;                // foo
            return () => {
              debugger;              // undefined
              return g.call("goo");  // undefined
            }
          };
        }
      };
    }
  };
}

// Stack-allocated receiver.
function g() {
  debugger;                        // goo
  return () => {
    debugger;                      // undefined
    with ({}) {
      return () => {
        debugger;                  // undefined
        try {
          throw new Error();
        } catch (e) {
          return () => {
            debugger;              // undefined
            return f.call("foo");  // undefined
          };
        }
      };
    }
  };
}

Debug.setListener(listener);

var h = f.call("foo");
for (var i = 0; i < 20; i++) h = h();
var h = g.call("goo");
for (var i = 0; i < 20; i++) h = h();

function x() {
  (() => this);      // bind this.
  function y() {
    (() => {
      (() => this);  // bind this.
      debugger;      // Y
     })();           // Y
  }
  y.call("Y");       // X
}
x.call("X");

function u() {
  (() => this);
  function v() {
    (() => {
      debugger;      // undefined
     })();           // V
  }
  v.call("V");       // U
}
u.call("U");

(() => {
  (() => this);
  debugger;          // [object global]
})();

Debug.setListener(null);

assertEquals(55, break_count);
assertNull(exception);
