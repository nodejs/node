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
    exec_state.prepareStep(Debug.StepAction.StepIn);
  }
}

Debug.setListener(listener);


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
