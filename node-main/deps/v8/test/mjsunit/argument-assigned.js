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

(function() {
  function f(x) {
    var arguments = [ 1, 2, 3 ];
    return x;
  }
  assertEquals(7, f(7));
})();


(function() {
  function f(x) {
    arguments[0] = 991;
    var arguments = [ 1, 2, 3 ];
    return x;
  }
  assertEquals(991, f(7));
})();


(function() {
  function f(x) {
    arguments[0] = 991;
    for (var i = 0; i < 10; i++) {
      if (i == 5) {
        var arguments = [ 1, 2, 3 ];
      }
    }
    return x;
  }
  assertEquals(991, f(7));
})();


(function() {
  function f(x, s) {
    eval(s);
    return x;
  }
  assertEquals(7, f(7, "var arguments = [ 1, 2, 3 ];"));
})();


(function() {
  function f(x, s) {
    var tmp = arguments[0];
    eval(s);
    return tmp;
  }
  assertEquals(7, f(7, "var arguments = [ 1, 2, 3 ];"));
})();


(function() {
  function f(x, s) {
    var tmp = arguments[0];
    eval(s);
    return tmp;
  }
  assertEquals(7, f(7, ""));
})();


(function() {
  function f(x, s) {
    var tmp = arguments[0];
    eval(s);
    return x;
  }
  assertEquals(7, f(7, "var arguments = [ 1, 2, 3 ];"));
})();


(function() {
  function f(x, s) {
    var tmp = arguments[0];
    eval(s);
    return x;
  }
  assertEquals(7, f(7, ""));
})();


(function() {
  function f(x) {
    function g(y) {
      x = y;
    }
    arguments = {};
    g(991);
    return x;
  }
  assertEquals(991, f(7));
})();


(function() {
  function f(x) {
    function g(y, s) {
      eval(s);
    }
    arguments = {};
    g(991, "x = y;");
    return x;
  }
  assertEquals(991, f(7));
})();
