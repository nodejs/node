// Copyright 2009 the V8 project authors. All rights reserved.
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

// Test handling of const variables in various settings.

(function () {
  function f() {
    function g() {
      x = 42;  //  should be ignored
      return x;  // force x into context
    }
    x = 43;  // should be ignored
    assertEquals(undefined, g());
    x = 44;  // should be ignored
    const x = 0;
    x = 45;  // should be ignored
    assertEquals(0, g());
  }
  f();
})();


(function () {
  function f() {
    function g() {
      with ({foo: 0}) {
        x = 42;  //  should be ignored
        return x;  // force x into context
      }
    }
    x = 43;  // should be ignored
    assertEquals(undefined, g());
    x = 44;  // should be ignored
    const x = 0;
    x = 45;  // should be ignored
    assertEquals(0, g());
  }
  f();
})();


(function () {
  function f() {
    function g(s) {
      eval(s);
      return x;  // force x into context
    }
    x = 43;  // should be ignored
    assertEquals(undefined, g("x = 42;"));
    x = 44;  // should be ignored
    const x = 0;
    x = 45;  // should be ignored
    assertEquals(0, g("x = 46;"));
  }
  f();
})();


(function () {
  function f() {
    function g(s) {
      with ({foo: 0}) {
        eval(s);
        return x;  // force x into context
      }
    }
    x = 43;  // should be ignored
    assertEquals(undefined, g("x = 42;"));
    x = 44;  // should be ignored
    const x = 0;
    x = 45;  // should be ignored
    assertEquals(0, g("x = 46;"));
  }
  f();
})();


(function () {
  function f(s) {
    function g() {
      x = 42;  // assign to global x, or to const x
      return x;
    }
    x = 43;  // declare global x
    assertEquals(42, g());
    x = 44;  // assign to global x
    eval(s);
    x = 45;  // should be ignored (assign to const x)
    assertEquals(0, g());
  }
  f("const x = 0;");
})();


(function () {
  function f(s) {
    function g() {
      with ({foo: 0}) {
        x = 42;  // assign to global x, or to const x
        return x;
      }
    }
    x = 43;  // declare global x
    assertEquals(42, g());
    x = 44;  // assign to global x
    eval(s);
    x = 45;  // should be ignored (assign to const x)
    assertEquals(0, g());
  }
  f("const x = 0;");
})();


(function () {
  function f(s) {
    function g(s) {
      eval(s);
      return x;
    }
    x = 43;  // declare global x
    assertEquals(42, g("x = 42;"));
    x = 44;  // assign to global x
    eval(s);
    x = 45;  // should be ignored (assign to const x)
    assertEquals(0, g("x = 46;"));
  }
  f("const x = 0;");
})();


(function () {
  function f(s) {
    function g(s) {
      with ({foo: 0}) {
        eval(s);
        return x;
      }
    }
    x = 43;  // declare global x
    assertEquals(42, g("x = 42;"));
    x = 44;  // assign to global x
    eval(s);
    x = 45;  // should be ignored (assign to const x)
    assertEquals(0, g("x = 46;"));
  }
  f("const x = 0;");
})();

