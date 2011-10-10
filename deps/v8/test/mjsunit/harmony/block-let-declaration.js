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

// Flags: --harmony-block-scoping

// Test let declarations in various settings.

// Global
let x;
let y = 2;

// Block local
{
  let y;
  let x = 3;
}

assertEquals(undefined, x);
assertEquals(2,y);

if (true) {
  let y;
  assertEquals(undefined, y);
}

// Invalid declarations are early errors in harmony mode and thus should trigger
// an exception in eval code during parsing, before even compiling or executing
// the code. Thus the generated function is not called here.
function TestLocalThrows(str, expect) {
  assertThrows("(function(){" + str + "})", expect);
}

function TestLocalDoesNotThrow(str) {
  assertDoesNotThrow("(function(){" + str + "})()");
}

// Test let declarations statement positions.
TestLocalThrows("if (true) let x;", SyntaxError);
TestLocalThrows("if (true) {} else let x;", SyntaxError);
TestLocalThrows("do let x; while (false)", SyntaxError);
TestLocalThrows("while (false) let x;", SyntaxError);
TestLocalThrows("label: let x;", SyntaxError);
TestLocalThrows("for (;false;) let x;", SyntaxError);
TestLocalThrows("switch (true) { case true: let x; }", SyntaxError);
TestLocalThrows("switch (true) { default: let x; }", SyntaxError);

// Test var declarations statement positions.
TestLocalDoesNotThrow("if (true) var x;");
TestLocalDoesNotThrow("if (true) {} else var x;");
TestLocalDoesNotThrow("do var x; while (false)");
TestLocalDoesNotThrow("while (false) var x;");
TestLocalDoesNotThrow("label: var x;");
TestLocalDoesNotThrow("for (;false;) var x;");
TestLocalDoesNotThrow("switch (true) { case true: var x; }");
TestLocalDoesNotThrow("switch (true) { default: var x; }");

// Test function declarations in source element and
// non-strict statement positions.
function f() {
  // Non-strict source element positions.
  function g0() {
    "use strict";
    // Strict source element positions.
    function h() { }
    {
      function h1() { }
    }
  }
  {
    function g1() { }
  }
  // Non-strict statement positions.
  if (true) function g2() { }
  if (true) {} else function g3() { }
  do function g4() { } while (false)
  while (false) function g5() { }
  label: function g6() { }
  for (;false;) function g7() { }
  switch (true) { case true: function g8() { } }
  switch (true) { default: function g9() { } }
}
f();

// Test function declarations in statement position in strict mode.
TestLocalThrows("function f() { 'use strict'; if (true) function g() {}", SyntaxError);
TestLocalThrows("function f() { 'use strict'; if (true) {} else function g() {}", SyntaxError);
TestLocalThrows("function f() { 'use strict'; do function g() {} while (false)", SyntaxError);
TestLocalThrows("function f() { 'use strict'; while (false) function g() {}", SyntaxError);
TestLocalThrows("function f() { 'use strict'; label: function g() {}", SyntaxError);
TestLocalThrows("function f() { 'use strict'; for (;false;) function g() {}", SyntaxError);
TestLocalThrows("function f() { 'use strict'; switch (true) { case true: function g() {} }", SyntaxError);
TestLocalThrows("function f() { 'use strict'; switch (true) { default: function g() {} }", SyntaxError);
