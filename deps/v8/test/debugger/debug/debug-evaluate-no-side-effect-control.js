// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --turbo

Debug = debug.Debug

var exception = null;

var o = { p : 1 };

var successes = [
  [45,
   `(function() {
      var sum = 0;
      for (var i = 0; i < 10; i++) sum += i;
      return sum;
    })()`
  ],
  ["0012",
   `(function() {
      var sum = 0;
      for (var i in [1, 2, 3]) sum += i;
      return sum;
    })()`
  ],
  [15,
   `(function() {
      var sum = 1;
      while (sum < 12) sum += sum + 1;
      return sum;
    })()`
  ],
  [15,
   `(function() {
      var sum = 1;
      do { sum += sum + 1; } while (sum < 12);
      return sum;
    })()`
  ],
  ["023",
   `(function() {
      var sum = "";
      for (var i = 0; i < 4; i++) {
        switch (i) {
          case 0:
          case 1:
            if (i == 0) sum += i;
            break;
          default:
          case 3:
            sum += i;
            break;
        }
      }
      return sum;
    })()`
  ],
  ["oups",
   `(function() {
      try {
        if (Math.sin(1) < 1) throw new Error("oups");
      } catch (e) {
        return e.message;
      }
    })()`
  ],
];

var fails = [
  `(function() {  // Iterator.prototype.next performs stores.
     var sum = 0;
     for (let i of [1, 2, 3]) sum += i;
     return sum;
   })()`,
  `(function() {  // Store to scope object.
     with (o) {
       p = 2;
     }
   })()`,
];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    successes.forEach(function ([expectation, source]) {
      assertEquals(expectation,
                   exec_state.frame(0).evaluate(source, true).value());
    });
    fails.forEach(function (test) {
      assertThrows(() => exec_state.frame(0).evaluate(test, true), EvalError);
    });
  } catch (e) {
    exception = e;
    print(e, e.stack);
  };
};

// Add the debug event listener.
Debug.setListener(listener);

function f() {
  debugger;
};

f();

assertNull(exception);
