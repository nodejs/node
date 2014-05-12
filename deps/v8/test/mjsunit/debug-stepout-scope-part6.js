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

var q = 42;
var prefixes = [ "debugger; ",
                 "if (false) { try { throw 0; } catch(x) { this.x = x; } }; debugger; " ];
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


// Test global eval and function constructor.
for (var i = 0; i < prefixes.length; ++i) {
  var pre = prefixes[i];
  for (var j = 0; j < bodies.length; ++j) {
    var body = bodies[j];
    eval("'use strict'; " + pre + body);
  }
}
