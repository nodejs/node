// Copyright 2008 the V8 project authors. All rights reserved.
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

// const variables should be read-only
const c = 42;
c = 87;
assertEquals(42, c);


// const variables are not behaving like other JS variables when it comes
// to scoping - in fact they behave more sanely. Inside a 'with' they do
// not interfere with the 'with' scopes.

(function () {
  with ({ x: 42 }) {
    const x = 7;
  }
  x = 5;
  assertEquals(7, x);
})();


// const variables may be declared but never initialized, in which case
// their value is undefined.

(function (sel) {
  if (sel == 0)
    with ({ x: 42 }) {
    const x;
    }
  else
    x = 3;
  x = 5;
  assertTrue(typeof x == 'undefined');
})(1);


// const variables may be initialized to undefined.
(function () {
  with ({ x: 42 }) {
    const x = undefined;
  }
  x = 5;
  assertTrue(typeof x == 'undefined');
})();


// const variables may be accessed in inner scopes like any other variable.
(function () {
  function bar() {
    assertEquals(7, x);
  }
  with ({ x: 42 }) {
    const x = 7;
  }
  x = 5
  bar();
})();


// const variables may be declared via 'eval'
(function () {
  with ({ x: 42 }) {
    eval('const x = 7');
  }
  x = 5;
  assertEquals(7, x);
})();
