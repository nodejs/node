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

listenerComplete = false;
exception = false;

// var0:   init after break point, changed by debug eval.
// const0: init before break point, changed by debug eval.
// const1: init after break point, materialized but untouched by debug eval.
// const2: init after break point, materialized and changed by debug eval.
// const3: context allocated const, init before break point, changed by eval.
function f() {
  var var1 = 21;
  const const3 = 3;

  function g() {
    const const0 = 0;
    assertEquals(undefined, const1);
    assertEquals(undefined, const2);
    assertEquals(3, const3);
    assertEquals(21, var1);

    debugger;  // Break point.

    assertEquals(30, var0);
    // TODO(yangguo): debug evaluate should not be able to alter
    //                stack-allocated const values
    // assertEquals(0, const0);
    assertEquals(undefined, const1);
    assertEquals(undefined, const2);
    var var0 = 20;
    const const1 = 1;
    const const2 = 2;
    assertEquals(20, var0);
    assertEquals(1, const1);
    assertEquals(2, const2);
  }

  g();

  assertEquals(31, var1);
  assertEquals(3, const3);
}


function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var frame = exec_state.frame(0);
    var evaluate = function(something) {
      return frame.evaluate(something).value()
    }

    var count = frame.localCount();
    assertEquals(4, count);
    var expectation = { "const0" : 0,
                        "const1" : undefined,
                        "const2" : undefined,
                        "const3" : 3,
                        "var0"   : undefined,
                        "var1"   : 21 };
    for (var i = 0; i < frame.localCount(); ++i) {
      var name = frame.localName(i);
      var value = frame.localValue(i).value();
      assertEquals(expectation[name], value);
    }

    evaluate('const0 = 10');
    evaluate('const2 = 12');
    evaluate('const3 = 13');
    evaluate('var0 = 30');
    evaluate('var1 = 31');

    // Indicate that all was processed.
    listenerComplete = true;
  } catch (e) {
    exception = e;
    print("Caught something. " + e + " " + e.stack);
  };
};

// Run and compile before debugger is active.
try { f(); } catch (e) { }

Debug.setListener(listener);

f();

Debug.setListener(null);

assertFalse(exception, "exception in listener")
assertTrue(listenerComplete);
