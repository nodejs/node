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

// Test for-of syntax.

"use strict";

function f() { for (x of y) { } }
function f() { for (var x of y) { } }
function f() { for (let x of y) { } }

function StrictSyntaxError(s) {
  try {
    eval(s);
  } catch (e) {
    assertInstanceof(e, SyntaxError);
    return;
  }
  throw "did not throw";
}

StrictSyntaxError("function f() { for (x of) { } }");
StrictSyntaxError("function f() { for (x of y z) { } }");
StrictSyntaxError("function f() { for (x of y;) { } }");

StrictSyntaxError("function f() { for (var x of) { } }");
StrictSyntaxError("function f() { for (var x of y z) { } }");
StrictSyntaxError("function f() { for (var x of y;) { } }");

StrictSyntaxError("function f() { for (let x of) { } }");
StrictSyntaxError("function f() { for (let x of y z) { } }");
StrictSyntaxError("function f() { for (let x of y;) { } }");

StrictSyntaxError("function f() { for (of y) { } }");
StrictSyntaxError("function f() { for (of of) { } }");
StrictSyntaxError("function f() { for (var of y) { } }");
StrictSyntaxError("function f() { for (var of of) { } }");
StrictSyntaxError("function f() { for (let of y) { } }");
StrictSyntaxError("function f() { for (let of of) { } }");

StrictSyntaxError("function f() { for (x = 3 of y) { } }");
StrictSyntaxError("function f() { for (var x = 3 of y) { } }");
StrictSyntaxError("function f() { for (let x = 3 of y) { } }");


// Alack, this appears to be valid.
function f() { for (of of y) { } }
function f() { for (let of of y) { } }
function f() { for (var of of y) { } }

// This too, of course.
function f() { for (of in y) { } }
function f() { for (var of in y) { } }
function f() { for (let of in y) { } }
