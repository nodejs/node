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

function CheckStrictMode(code, exception) {
  assertDoesNotThrow(code);
  assertThrows("'use strict';\n" + code, exception);
  assertThrows('"use strict";\n' + code, exception);
  assertDoesNotThrow("\
    function outer() {\
      function inner() {\n"
        + code +
      "\n}\
    }");
  assertThrows("\
    function outer() {\
      'use strict';\
      function inner() {\n"
        + code +
      "\n}\
    }", exception);
}

// Incorrect 'use strict' directive.
function UseStrictEscape() {
  "use\\x20strict";
  with ({}) {};
}

// 'use strict' in non-directive position.
function UseStrictNonDirective() {
  void(0);
  "use strict";
  with ({}) {};
}

// Multiple directives, including "use strict".
assertThrows('\
"directive 1";\
"another directive";\
"use strict";\
"directive after strict";\
"and one more";\
with({}) {}', SyntaxError);

// 'with' disallowed in strict mode.
CheckStrictMode("with({}) {}", SyntaxError);

// Function named 'eval'.
CheckStrictMode("function eval() {}", SyntaxError)

// Function named 'arguments'.
CheckStrictMode("function arguments() {}", SyntaxError)

// Function parameter named 'eval'.
//CheckStrictMode("function foo(a, b, eval, c, d) {}", SyntaxError)

// Function parameter named 'arguments'.
//CheckStrictMode("function foo(a, b, arguments, c, d) {}", SyntaxError)

// Property accessor parameter named 'eval'.
//CheckStrictMode("var o = { set foo(eval) {} }", SyntaxError)

// Property accessor parameter named 'arguments'.
//CheckStrictMode("var o = { set foo(arguments) {} }", SyntaxError)

// Duplicate function parameter name.
//CheckStrictMode("function foo(a, b, c, d, b) {}", SyntaxError)

// catch(eval)
CheckStrictMode("try{}catch(eval){};", SyntaxError)

// catch(arguments)
CheckStrictMode("try{}catch(arguments){};", SyntaxError)

// var eval
CheckStrictMode("var eval;", SyntaxError)

// var arguments
CheckStrictMode("var arguments;", SyntaxError)

// Strict mode applies to the function in which the directive is used..
//assertThrows('\
//function foo(eval) {\
//  "use strict";\
//}', SyntaxError);

// Strict mode doesn't affect the outer stop of strict code.
function NotStrict(eval) {
  function Strict() {
    "use strict";
  }
  with ({}) {};
}
