// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --min-preparse-length=0

// The test functions in this file will be eagerly compiled. The functions
// inside will be eagerly parsed but lazily compiled.

(function TestLengths() {
  function inner(p1, p2, p3) { }
  assertEquals(3, inner.length);
})();

(function TestAccessingContextVariables() {
  var in_context = 8;
  function inner() { return in_context; }
  assertEquals(8, inner());
})();

(function TestAccessingContextVariablesFromDeeper() {
  var in_context = 8;
  function inner() {
    function inner_inner() {
      function inner_inner_inner() {
        return in_context;
      }
      return inner_inner_inner;
    }
    return inner_inner;
  }
  assertEquals(8, inner()()());
})();
