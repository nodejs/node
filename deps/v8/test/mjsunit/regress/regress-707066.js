// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// There was a bug in CreateDynamicFunction where a stack overflow
// situation caused an assertion failure.

function test(api) {
  function f() {
    try {
      // induce a stack overflow
      f();
    } catch(e) {
      // this might result in even more stack overflows
      api();
    }
  }
  f();
}

test((      function (){}).constructor); // Function
test((      function*(){}).constructor); // GeneratorFunction
test((async function (){}).constructor); // AsyncFunction
