// Copyright 2012 the V8 project authors. All rights reserved.
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


"use strict";
let top_level_let = 255;

var Debug = debug.Debug;

const ScopeType = debug.ScopeType;

let exception = null;
let listenerDelegate = null;

const expected_break_count = 5;
let break_count = 0;

Debug.setListener(function(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    break_count++;
    listenerDelegate(exec_state);
  } catch (e) {
    exception = e;
    print(e, e.stack);
  }
});

function CheckScope(scope_frame, scope_expectations, expected_scope_type) {
  assertEquals(expected_scope_type, scope_frame.scopeType());

  var scope_object = scope_frame.scopeObject().value();

  for (let name in scope_expectations) {
    let actual = scope_object[name];
    let expected = scope_expectations[name];
    assertEquals(expected, actual);
  }
}

// ---

listenerDelegate = function(exec_state) {
  const frame = exec_state.frame(0);

  assertEquals(6, frame.scopeCount());

  CheckScope(frame.scope(0), {}, ScopeType.Local);
  CheckScope(frame.scope(1), { a: 4, b: 5 }, ScopeType.Closure);
  CheckScope(frame.scope(2), { z: 22, w: 5, v: "Capybara" }, ScopeType.Closure);
  CheckScope(frame.scope(3), { x: 5 }, ScopeType.Closure);
  CheckScope(frame.scope(4), { top_level_let: 255 }, ScopeType.Script);
  CheckScope(frame.scope(5), {}, ScopeType.Global);
};

(function F1(x) {
  function F2(y) {
    var z = x + y;
    {
      var w =  5;
      var v = "Capybara";
      var F3 = function(a, b) {
        function F4(p) {
          debugger;
          return p + a + b + z + w + v.length;
        }
        return F4;
      }
      return F3(4, 5);
    }
  }
  return F2(17);
})(5)();

// ---

listenerDelegate = function(exec_state) {
  const frame = exec_state.frame(0);

  assertEquals(6, frame.scopeCount());

  CheckScope(frame.scope(0), {}, ScopeType.Local);
  CheckScope(frame.scope(1), { l3: 9 }, ScopeType.Block);
  CheckScope(frame.scope(2), { l2: 7 }, ScopeType.Block);
  CheckScope(frame.scope(3), { v1:3, l0: 0, v3: 5, v6: 11 }, ScopeType.Closure);
  CheckScope(frame.scope(4), { top_level_let: 255 }, ScopeType.Script);
  CheckScope(frame.scope(5), {}, ScopeType.Global);
};

(function() {
  var v1 = 3;
  var v2 = 4;
  let l0 = 0;
  {
    var v3 = 5;
    let l1 = 6;
    let l2 = 7;
    {
      var v4 = 8;
      let l3 = 9;
      {
        var v5 = "Cat";
        let l4 = 11;
        var v6 = l4;
        return function() {
          debugger;
          return l0 + v1 + v3 + l2 + l3 + v6;
        };
      }
    }
  }
})()();
