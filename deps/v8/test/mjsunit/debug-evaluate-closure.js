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

// Flags: --expose-debug-as debug --allow-natives-syntax

Debug = debug.Debug;
var listened = false;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    assertEquals("goo", exec_state.frame(0).evaluate("goo").value());
    exec_state.frame(0).evaluate("goo = 'goo foo'");
    assertEquals("bar return", exec_state.frame(0).evaluate("bar()").value());
    assertEquals("inner bar", exec_state.frame(0).evaluate("inner").value());
    assertEquals("outer bar", exec_state.frame(0).evaluate("outer").value());

    assertEquals("baz inner", exec_state.frame(0).evaluate("baz").value());
    assertEquals("baz outer", exec_state.frame(1).evaluate("baz").value());
    exec_state.frame(0).evaluate("w = 'w foo'");
    exec_state.frame(0).evaluate("inner = 'inner foo'");
    exec_state.frame(0).evaluate("outer = 'outer foo'");
    exec_state.frame(0).evaluate("baz = 'baz inner foo'");
    exec_state.frame(1).evaluate("baz = 'baz outer foo'");
    listened = true;
  } catch (e) {
    print(e);
    print(e.stack);
  }
}

Debug.setListener(listener);

var outer = "outer";
var baz = "baz outer";

function foo() {
  var inner = "inner";
  var baz = "baz inner";
  var goo = "goo";
  var withw = { w: "w" };
  var withv = { v: "v" };

  with (withv) {
    var bar = function bar() {
      assertEquals("goo foo", goo);
      inner = "inner bar";
      outer = "outer bar";
      v = "v bar";
      return "bar return";
    };
  }

  with (withw) {
    debugger;
  }

  assertEquals("inner foo", inner);
  assertEquals("baz inner foo", baz);
  assertEquals("w foo", withw.w);
  assertEquals("v bar", withv.v);
}

foo();
assertEquals("outer foo", outer);
assertEquals("baz outer foo", baz);
assertTrue(listened);
Debug.setListener(null);
