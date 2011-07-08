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

// Flags: --allow-natives-syntax

var global = 0;
var MAX = 5;

// Attempt to inline strcit in non-strict.

function strictToBeInlined(n) {
  "use strict";
  global = "strict";
  if (n == MAX) { undefined_variable_strict = "value"; }
}

function nonstrictCallStrict(n) {
  strictToBeInlined(n);
}

(function testInlineStrictInNonStrict() {
  for (var i = 0; i <= MAX; i ++) {
    try {
      if (i == MAX - 1) %OptimizeFunctionOnNextCall(nonstrictCallStrict);
      nonstrictCallStrict(i);
    } catch (e) {
      assertInstanceof(e, ReferenceError);
      assertEquals(MAX, i);
      return;
    }
  }
  fail("ReferenceError after MAX iterations", "no exception");
})();

// Attempt to inline non-strict in strict.

function nonstrictToBeInlined(n) {
  global = "nonstrict";
  if (n == MAX) { undefined_variable_nonstrict = "The nonstrict value"; }
}

function strictCallNonStrict(n) {
  "use strict";
  nonstrictToBeInlined(n);
}

(function testInlineNonStrictInStrict() {
  for (var i = 0; i <= MAX; i ++) {
    try {
      if (i == MAX - 1) %OptimizeFunctionOnNextCall(nonstrictCallStrict);
      strictCallNonStrict(i);
    } catch (e) {
      fail("no exception", "exception");
    }
  }
  assertEquals("The nonstrict value", undefined_variable_nonstrict);
})();

// Optimize strict function.

function strictAssignToUndefined(n) {
  "use strict";
  global = "strict";
  if (n == MAX) { undefined_variable_strict_2 = "value"; }
}

(function testOptimizeStrictAssignToUndefined() {
  for (var i = 0; i <= MAX; i ++) {
    try {
      if (i == MAX - 1) %OptimizeFunctionOnNextCall(nonstrictCallStrict);
      strictAssignToUndefined(i);
    } catch (e) {
      assertInstanceof(e, ReferenceError);
      assertEquals(MAX, i);
      return;
    }
  }
  fail("ReferenceError after MAX iterations", "no exception");
})();
