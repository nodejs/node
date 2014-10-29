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

// Flags: --harmony-scoping --use-strict

// Test for-of syntax.

"use strict";

function f() { for (x of y) { } }
function f() { for (var x of y) { } }
function f() { for (let x of y) { } }

assertThrows("function f() { for (x of) { } }", SyntaxError);
assertThrows("function f() { for (x of y z) { } }", SyntaxError);
assertThrows("function f() { for (x of y;) { } }", SyntaxError);

assertThrows("function f() { for (var x of) { } }", SyntaxError);
assertThrows("function f() { for (var x of y z) { } }", SyntaxError);
assertThrows("function f() { for (var x of y;) { } }", SyntaxError);

assertThrows("function f() { for (let x of) { } }", SyntaxError);
assertThrows("function f() { for (let x of y z) { } }", SyntaxError);
assertThrows("function f() { for (let x of y;) { } }", SyntaxError);

assertThrows("function f() { for (of y) { } }", SyntaxError);
assertThrows("function f() { for (of of) { } }", SyntaxError);
assertThrows("function f() { for (var of y) { } }", SyntaxError);
assertThrows("function f() { for (var of of) { } }", SyntaxError);
assertThrows("function f() { for (let of y) { } }", SyntaxError);
assertThrows("function f() { for (let of of) { } }", SyntaxError);

assertThrows("function f() { for (x = 3 of y) { } }", SyntaxError);
assertThrows("function f() { for (var x = 3 of y) { } }", SyntaxError);
assertThrows("function f() { for (let x = 3 of y) { } }", SyntaxError);


// Alack, this appears to be valid.
function f() { for (of of y) { } }
function f() { for (let of of y) { } }
function f() { for (var of of y) { } }

// This too, of course.
function f() { for (of in y) { } }
function f() { for (var of in y) { } }
function f() { for (let of in y) { } }
