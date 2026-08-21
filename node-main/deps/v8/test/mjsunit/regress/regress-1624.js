// Copyright 2012 the V8 project authors. All rights reserved.
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

// Test that global eval calls of strict code (independent from whether being
// direct or indirect) have their own lexical and variable environment.

var evil = eval;

// Test global direct strict eval.
// Expects new environment.
var no_touch = 0;
eval('"use strict"; var no_touch = 1;');
assertSame(0, no_touch);

// Test global indirect strict eval.
// Expects new environment.
var no_touch = 0;
evil('"use strict"; var no_touch = 2;');
assertSame(0, no_touch);

// Test global direct non-strict eval.
// Expects global environment.
var no_touch = 0;
eval('var no_touch = 3;');
assertSame(3, no_touch);

// Test global indirect non-strict eval.
// Expects global environment.
var no_touch = 0;
evil('var no_touch = 4;');
assertSame(4, no_touch);

// Test non-global direct strict eval in non-strict function.
// Expects new environment.
var no_touch = 0;
(function() {
  var no_touch = 0;
  eval('"use strict"; var no_touch = 5;');
  assertSame(0, no_touch);
})()
assertSame(0, no_touch);

// Test non-global indirect strict eval in non-strict function.
// Expects new environment.
var no_touch = 0;
(function() {
  var no_touch = 0;
  evil('"use strict"; var no_touch = 6;');
  assertSame(0, no_touch);
})()
assertSame(0, no_touch);

// Test non-global direct non-strict eval in non-strict function.
// Expects function environment.
var no_touch = 0;
(function() {
  var no_touch = 0;
  eval('var no_touch = 7;');
  assertSame(7, no_touch);
})()
assertSame(0, no_touch);

// Test non-global indirect non-strict eval in non-strict function.
// Expects global environment.
var no_touch = 0;
(function() {
  var no_touch = 0;
  evil('var no_touch = 8;');
  assertSame(0, no_touch);
})()
assertSame(8, no_touch);

// Test non-global direct strict eval in strict function.
// Expects new environment.
var no_touch = 0;
(function() {
  "use strict";
  var no_touch = 0;
  eval('"use strict"; var no_touch = 9;');
  assertSame(0, no_touch);
})()
assertSame(0, no_touch);

// Test non-global indirect strict eval in strict function.
// Expects new environment.
var no_touch = 0;
(function() {
  "use strict";
  var no_touch = 0;
  evil('"use strict"; var no_touch = 10;');
  assertSame(0, no_touch);
})()
assertSame(0, no_touch);

// Test non-global direct non-strict eval in strict function.
// Expects new environment.
var no_touch = 0;
(function() {
  "use strict";
  var no_touch = 0;
  eval('var no_touch = 11;');
  assertSame(0, no_touch);
})()
assertSame(0, no_touch);

// Test non-global indirect non-strict eval in strict function.
// Expects global environment.
var no_touch = 0;
(function() {
  "use strict";
  var no_touch = 0;
  evil('var no_touch = 12;');
  assertSame(0, no_touch);
})()
assertSame(12, no_touch);
