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

// Get the Debug object exposed from the debug context global object.
var Debug = debug.Debug;

function CheckScope(scope_mirror, scope_expectations, expected_scope_type) {
  assertEquals(expected_scope_type, scope_mirror.scopeType());

  var scope_object = scope_mirror.scopeObject().value();

  for (var name in scope_expectations) {
    var actual = scope_object[name];
    var expected = scope_expectations[name];
    assertEquals(expected, actual);
  }
}

var ScopeType = debug.ScopeType;

var f1 = (function F1(x) {
  function F2(y) {
    var z = x + y;
    with ({w: 5, v: "Capybara"}) {
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

assertEquals(6, mirror.scopeCount());

CheckScope(mirror.scope(0), { a: 4, b: 5 }, ScopeType.Closure);
CheckScope(mirror.scope(1), { w: 5, v: "Capybara" }, ScopeType.With);
CheckScope(mirror.scope(2), { z: 22 }, ScopeType.Closure);
CheckScope(mirror.scope(3), { x: 5 }, ScopeType.Closure);
CheckScope(mirror.scope(4), {}, ScopeType.Script);
CheckScope(mirror.scope(5), {}, ScopeType.Global);

var f2 = function() { return 5; }

var mirror = Debug.MakeMirror(f2);

assertEquals(2, mirror.scopeCount());

CheckScope(mirror.scope(0), {}, ScopeType.Script);
CheckScope(mirror.scope(1), {}, ScopeType.Global);

var f3 = (function F1(invisible_parameter) {
  var invisible1 = 1;
  var visible1 = 10;
  return (function F2() {
    var invisible2 = 2;
    return (function F3() {
      var visible2 = 20;
      return (function () {return visible1 + visible2 + visible1a;});
    })();
  })();
})(5);

var mirror = Debug.MakeMirror(f3);

assertEquals(4, mirror.scopeCount());

CheckScope(mirror.scope(0), { visible2: 20 }, ScopeType.Closure);
CheckScope(mirror.scope(1), { visible1: 10 }, ScopeType.Closure);
CheckScope(mirror.scope(2), {}, ScopeType.Script);
CheckScope(mirror.scope(3), {}, ScopeType.Global);


var f4 = (function One() {
  try {
    throw "I'm error 1";
  } catch (e1) {
    try {
      throw "I'm error 2";
    } catch (e2) {
      return function GetError() {
        return e1 + e2;
      };
    }
  }
})();

var mirror = Debug.MakeMirror(f4);

assertEquals(4, mirror.scopeCount());

CheckScope(mirror.scope(0), { e2: "I'm error 2" }, ScopeType.Catch);
CheckScope(mirror.scope(1), { e1: "I'm error 1" }, ScopeType.Catch);
CheckScope(mirror.scope(2), {}, ScopeType.Script);
CheckScope(mirror.scope(3), {}, ScopeType.Global);


var f5 = (function Raz(p1, p2) {
  var p3 = p1 + p2;
  return (function() {
    var p4 = 20;
    var p5 = 21;
    var p6 = 22;
    return eval("(function(p7){return p1 + p4 + p6 + p7})");
  })();
})(1,2);

var mirror = Debug.MakeMirror(f5);

assertEquals(4, mirror.scopeCount());

CheckScope(mirror.scope(0), { p4: 20, p6: 22 }, ScopeType.Closure);
CheckScope(mirror.scope(1), { p1: 1 }, ScopeType.Closure);
CheckScope(mirror.scope(2), {}, ScopeType.Script);
CheckScope(mirror.scope(3), {}, ScopeType.Global);


function CheckNoScopeVisible(f) {
  var mirror = Debug.MakeMirror(f);
  assertEquals(0, mirror.scopeCount());
}

CheckNoScopeVisible(Number);

CheckNoScopeVisible(Function.toString);
