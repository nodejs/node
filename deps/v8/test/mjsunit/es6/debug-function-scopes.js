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

// Flags: --expose-debug-as debug

"use strict";
let top_level_let = 255;

// Get the Debug object exposed from the debug context global object.
var Debug = debug.Debug;

function CheckScope(scope_mirror, scope_expectations, expected_scope_type) {
  assertEquals(expected_scope_type, scope_mirror.scopeType());

  var scope_object = scope_mirror.scopeObject().value();

  for (let name in scope_expectations) {
    let actual = scope_object[name];
    let expected = scope_expectations[name];
    assertEquals(expected, actual);
  }
}

// A copy of the scope types from debug/mirrors.js.
var ScopeType = { Global: 0,
                  Local: 1,
                  With: 2,
                  Closure: 3,
                  Catch: 4,
                  Block: 5,
                  Script: 6};

var f1 = (function F1(x) {
  function F2(y) {
    var z = x + y;
    {
      var w =  5;
      var v = "Capybara";
      var F3 = function(a, b) {
        function F4(p) {
          return p + a + b + z + w + v.length;
        }
        return F4;
      }
      return F3(4, 5);
    }
  }
  return F2(17);
})(5);

var mirror = Debug.MakeMirror(f1);

assertEquals(5, mirror.scopeCount());

CheckScope(mirror.scope(0), { a: 4, b: 5 }, ScopeType.Closure);
CheckScope(mirror.scope(1), { z: 22, w: 5, v: "Capybara" }, ScopeType.Closure);
CheckScope(mirror.scope(2), { x: 5 }, ScopeType.Closure);
CheckScope(mirror.scope(3), { top_level_let: 255 }, ScopeType.Script);
CheckScope(mirror.scope(4), {}, ScopeType.Global);

var f2 = (function() {
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
          return l0 + v1 + v3 + l2 + l3 + v6;
        };
      }
    }
  }
})();

var mirror = Debug.MakeMirror(f2);

assertEquals(5, mirror.scopeCount());

CheckScope(mirror.scope(0), { l3: 9 }, ScopeType.Block);
CheckScope(mirror.scope(1), { l2: 7 }, ScopeType.Block);
CheckScope(mirror.scope(2), { v1:3, l0: 0, v3: 5, v6: 11 }, ScopeType.Closure);
CheckScope(mirror.scope(3), { top_level_let: 255 }, ScopeType.Script);
CheckScope(mirror.scope(4), {}, ScopeType.Global);
