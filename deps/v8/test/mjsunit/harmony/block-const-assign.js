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

// Flags: --harmony-scoping

// Test that we throw early syntax errors in harmony mode
// when using an immutable binding in an assigment or with
// prefix/postfix decrement/increment operators.

"use strict";

// Function local const.
function constDecl0(use) {
  return "(function() { const constvar = 1; " + use + "; });";
}


function constDecl1(use) {
  return "(function() { " + use + "; const constvar = 1; });";
}


// Function local const, assign from eval.
function constDecl2(use) {
  use = "eval('(function() { " + use + " })')";
  return "(function() { const constvar = 1; " + use + "; })();";
}


function constDecl3(use) {
  use = "eval('(function() { " + use + " })')";
  return "(function() { " + use + "; const constvar = 1; })();";
}


// Block local const.
function constDecl4(use) {
  return "(function() { { const constvar = 1; " + use + "; } });";
}


function constDecl5(use) {
  return "(function() { { " + use + "; const constvar = 1; } });";
}


// Block local const, assign from eval.
function constDecl6(use) {
  use = "eval('(function() {" + use + "})')";
  return "(function() { { const constvar = 1; " + use + "; } })();";
}


function constDecl7(use) {
  use = "eval('(function() {" + use + "})')";
  return "(function() { { " + use + "; const constvar = 1; } })();";
}


// Function expression name.
function constDecl8(use) {
  return "(function constvar() { " + use + "; });";
}


// Function expression name, assign from eval.
function constDecl9(use) {
  use = "eval('(function(){" + use + "})')";
  return "(function constvar() { " + use + "; })();";
}

let decls = [ constDecl0,
              constDecl1,
              constDecl2,
              constDecl3,
              constDecl4,
              constDecl5,
              constDecl6,
              constDecl7,
              constDecl8,
              constDecl9
              ];
let uses = [ 'constvar = 1;',
             'constvar += 1;',
             '++constvar;',
             'constvar++;'
             ];

function Test(d,u) {
  'use strict';
  try {
    print(d(u));
    eval(d(u));
  } catch (e) {
    assertInstanceof(e, SyntaxError);
    assertTrue(e.toString().indexOf("Assignment to constant variable") >= 0);
    return;
  }
  assertUnreachable();
}

for (var d = 0; d < decls.length; ++d) {
  for (var u = 0; u < uses.length; ++u) {
    Test(decls[d], uses[u]);
  }
}
