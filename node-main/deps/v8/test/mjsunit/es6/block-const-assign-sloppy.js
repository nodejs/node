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

// Test that we throw early syntax errors in harmony mode
// when using an immutable binding in an assigment or with
// prefix/postfix decrement/increment operators.

const decls = [
  // Const declaration.
  function(use) { return "const c = 1; " + use + ";" }, TypeError,
  function(use) { return "const x = 0, c = 1; " + use + ";" }, TypeError,
  function(use) { return "const c = 1, x = (" + use + ");" }, TypeError,
  function(use) { return use + "; const c = 1;" }, ReferenceError,
  function(use) { return use + "; const x = 0, c = 1;" }, ReferenceError,
  function(use) { return "const x = (" + use + "), c = 1;" }, ReferenceError,
  function(use) { return "const c = (" + use + ");" }, ReferenceError,

  // Function expression.
  function(use) { return "(function c() { " + use + "; })();"; }, TypeError,
  // TODO(rossberg): Once we have default parameters, test using 'c' there.

  // Class expression.
  function(use) {
    return "new class c { constructor() { " + use + " } };";
  }, TypeError,
  function(use) {
    return "(new class c { m() { " + use + " } }).m();";
  }, TypeError,
  function(use) {
    return "(new class c { get a() { " + use + " } }).a;";
  }, TypeError,
  function(use) {
    return "(new class c { set a(x) { " + use + " } }).a = 0;";
  }, TypeError,
  function(use) {
    return "(class c { static m() { " + use + " } }).s();";
  }, TypeError,
  function(use) {
    return "(class c extends (" + use + ") {});";
  }, ReferenceError,
  function(use) {
    return "(class c { [" + use + "]() {} });";
  }, ReferenceError,
  function(use) {
    return "(class c { get [" + use + "]() {} });";
  }, ReferenceError,
  function(use) {
    return "(class c { set [" + use + "](x) {} });";
  }, ReferenceError,
  function(use) {
    return "(class c { static [" + use + "]() {} });";
  }, ReferenceError,

  // For loop.
  function(use) {
    return "for (const c = 0; " + use + ";) {}"
  }, TypeError,
  function(use) {
    return "for (const x = 0, c = 0; " + use + ";) {}"
  }, TypeError,
  function(use) {
    return "for (const c = 0; ; " + use + ") {}"
  }, TypeError,
  function(use) {
    return "for (const x = 0, c = 0; ; " + use + ") {}"
  }, TypeError,
  function(use) {
    return "for (const c = 0; ;) { " + use + "; }"
  }, TypeError,
  function(use) {
    return "for (const x = 0, c = 0; ;) { " + use + "; }"
  }, TypeError,
  function(use) {
    return "for (const c in {a: 1}) { " + use + "; }"
  }, TypeError,
  function(use) {
    return "for (const c of [1]) { " + use + "; }"
  }, TypeError,
  function(use) {
    return "for (const x = (" + use + "), c = 0; ;) {}"
  }, ReferenceError,
  function(use) {
    return "for (const c = (" + use + "); ;) {}"
  }, ReferenceError,
]

let uses = [
  'c = 1',
  'c += 1',
  '++c',
  'c--',
];

let declcontexts = [
  function(decl) { return decl; },
  function(decl) { return "eval(\'" + decl + "\')"; },
  function(decl) { return "{ " + decl + " }"; },
  function(decl) { return "(function() { " + decl + " })()"; },
];

let usecontexts = [
  function(use) { return use; },
  function(use) { return "eval(\"" + use + "\")"; },
  function(use) { return "(function() { " + use + " })()"; },
  function(use) { return "(function() { eval(\"" + use + "\"); })()"; },
  function(use) { return "eval(\"(function() { " + use + "; })\")()"; },
];

function Test(program, error) {
  program = "'use strict'; " + program;
  try {
    print(program, "  // throw " + error.name);
    eval(program);
  } catch (e) {
    assertInstanceof(e, error);
    if (e === TypeError) {
      assertTrue(e.toString().indexOf("Assignment to constant variable") >= 0);
    }
    return;
  }
  assertUnreachable();
}

for (var d = 0; d < decls.length; d += 2) {
  for (var u = 0; u < uses.length; ++u) {
    for (var o = 0; o < declcontexts.length; ++o) {
      for (var i = 0; i < usecontexts.length; ++i) {
        Test(declcontexts[o](decls[d](usecontexts[i](uses[u]))), decls[d + 1]);
      }
    }
  }
}
