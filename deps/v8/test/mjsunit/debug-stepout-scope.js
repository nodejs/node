// Copyright 2011 the V8 project authors. All rights reserved.
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

// Flags: --expose-debug-as debug --expose-natives-as=builtins

// Check that the ScopeIterator can properly recreate the scope at
// every point when stepping through functions.

var Debug = debug.Debug;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    // Access scope details.
    var scope_count = exec_state.frame().scopeCount();
    for (var i = 0; i < scope_count; i++) {
      var scope = exec_state.frame().scope(i);
      // assertTrue(scope.isScope());
      scope.scopeType();
      scope.scopeObject();
    }

    // Do steps until we reach the global scope again.
    if (true) {
      exec_state.prepareStep(Debug.StepAction.StepInMin, 1);
    }
  }
}

Debug.setListener(listener);


function test1() {
  debugger;
  with ({x:1}) {
    x = 2;
  }
}
test1();


function test2() {
  if (true) {
    with ({}) {
      debugger;
    }
  } else {
    with ({}) {
      return 10;
    }
  }
}
test2();


function test3() {
  if (true) {
    debugger;
  } else {
    with ({}) {
      return 10;
    }
  }
}
test3();


function test4() {
  debugger;
  with ({x:1}) x = 1
}
test4();


function test5() {
  debugger;
  var dummy = 1;
  with ({}) {
    with ({}) {
      dummy = 2;
    }
  }
  dummy = 3;
}
test5();


function test6() {
  debugger;
  try {
    throw 'stuff';
  } catch (e) {
    e = 1;
  }
}
test6();


function test7() {
  debugger;
  function foo() {}
}
test7();


function test8() {
  debugger;
  (function foo() {})();
}
test8();


var q = 42;
var prefixes = [ "debugger; ",
                 "if (false) { try { throw 0; } catch(x) { return x; } }; debugger; " ];
var bodies = [ "1",
               "1 ",
               "1;",
               "1; ",
               "q",
               "q ",
               "q;",
               "q; ",
               "try { throw 'stuff' } catch (e) { e = 1; }",
               "try { throw 'stuff' } catch (e) { e = 1; } ",
               "try { throw 'stuff' } catch (e) { e = 1; };",
               "try { throw 'stuff' } catch (e) { e = 1; }; " ];
var with_bodies = [ "with ({}) {}",
                    "with ({x:1}) x",
                    "with ({x:1}) x = 1",
                    "with ({x:1}) x ",
                    "with ({x:1}) x = 1 ",
                    "with ({x:1}) x;",
                    "with ({x:1}) x = 1;",
                    "with ({x:1}) x; ",
                    "with ({x:1}) x = 1; " ];


function test9() {
  debugger;
  for (var i = 0; i < prefixes.length; ++i) {
    var pre = prefixes[i];
    for (var j = 0; j < bodies.length; ++j) {
      var body = bodies[j];
      eval(pre + body);
      eval("'use strict'; " + pre + body);
    }
    for (var j = 0; j < with_bodies.length; ++j) {
      var body = with_bodies[j];
      eval(pre + body);
    }
  }
}
test9();


function test10() {
  debugger;
  with ({}) {
    return 10;
  }
}
test10();


function test11() {
  debugger;
  try {
    throw 'stuff';
  } catch (e) {
    return 10;
  }
}
test11();


// Test global eval and function constructor.
for (var i = 0; i < prefixes.length; ++i) {
  var pre = prefixes[i];
  for (var j = 0; j < bodies.length; ++j) {
    var body = bodies[j];
    eval(pre + body);
    eval("'use strict'; " + pre + body);
    Function(pre + body)();
  }
  for (var j = 0; j < with_bodies.length; ++j) {
    var body = with_bodies[j];
    eval(pre + body);
    Function(pre + body)();
  }
}


try {
  with({}) {
    debugger;
    eval("{}$%:^");
  }
} catch(e) {
  nop();
}

// Return from function constructed with Function constructor.
var anon = 12;
for (var i = 0; i < prefixes.length; ++i) {
  var pre = prefixes[i];
  Function(pre + "return 42")();
  Function(pre + "return 42 ")();
  Function(pre + "return 42;")();
  Function(pre + "return 42; ")();
  Function(pre + "return anon")();
  Function(pre + "return anon ")();
  Function(pre + "return anon;")();
  Function(pre + "return anon; ")();
}


function nop() {}


function stress() {
  debugger;

  L: with ({x:12}) {
    break L;
  }


  with ({x: 'outer'}) {
    label: {
      with ({x: 'inner'}) {
        break label;
      }
    }
  }


  with ({x: 'outer'}) {
    label: {
      with ({x: 'inner'}) {
        break label;
      }
    }
    nop();
  }


  with ({x: 'outer'}) {
    label: {
      with ({x: 'middle'}) {
        with ({x: 'inner'}) {
          break label;
        }
      }
    }
  }


  with ({x: 'outer'}) {
    label: {
      with ({x: 'middle'}) {
        with ({x: 'inner'}) {
          break label;
        }
      }
    }
    nop();
  }


  with ({x: 'outer'}) {
    for (var i = 0; i < 3; ++i) {
      with ({x: 'inner' + i}) {
        continue;
      }
    }
  }


  with ({x: 'outer'}) {
    label: for (var i = 0; i < 3; ++i) {
      with ({x: 'middle' + i}) {
        for (var j = 0; j < 3; ++j) {
          with ({x: 'inner' + j}) {
            continue label;
          }
        }
      }
    }
  }


  with ({x: 'outer'}) {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } catch (e) {
    }
  }


  with ({x: 'outer'}) {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } catch (e) {
      nop();
    }
  }


  with ({x: 'outer'}) {
    try {
      with ({x: 'middle'}) {
        with ({x: 'inner'}) {
          throw 0;
        }
      }
    } catch (e) {
    }
  }


  try {
    with ({x: 'outer'}) {
      try {
        with ({x: 'inner'}) {
          throw 0;
        }
      } finally {
      }
    }
  } catch (e) {
  }


  try {
    with ({x: 'outer'}) {
      try {
        with ({x: 'inner'}) {
          throw 0;
        }
      } finally {
        nop();
      }
    }
  } catch (e) {
  }


  function stress1() {
    with ({x:12}) {
      return x;
    }
  }
  stress1();


  function stress2() {
    with ({x: 'outer'}) {
      with ({x: 'inner'}) {
        return x;
      }
    }
  }
  stress2();

  function stress3() {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } catch (e) {
      return e;
    }
  }
  stress3();


  function stress4() {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } catch (e) {
      with ({x: 'inner'}) {
        return e;
      }
    }
  }
  stress4();

}
stress();


// With block as the last(!) statement in global code.
with ({}) { debugger; }