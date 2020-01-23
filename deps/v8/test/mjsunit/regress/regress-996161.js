// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function checkOwnProperties(v, count) {
  var properties = Object.getOwnPropertyNames(v);
  assertEquals(properties.length, count);
}


function testStoreNoFeedback() {
  arr = new Int32Array(10);
  function f(a) { a["-1"] = 15; }

  for (var i = 0; i < 3; i++) {
    arr.__defineGetter__("x", function() { });
    checkOwnProperties(arr, 11);
    f(arr);
  }
}
testStoreNoFeedback();

function testStoreGeneric() {
  arr = new Int32Array(10);
  var index = "-1";
  function f1(a) { a[index] = 15; }
  %EnsureFeedbackVectorForFunction(f1);

  // Make a[index] in f1 megamorphic
  f1({a: 1});
  f1({b: 1});
  f1({c: 1});
  f1({d: 1});

  for (var i = 0; i < 3; i++) {
    arr.__defineGetter__("x", function() { });
    checkOwnProperties(arr, 11);
    f1(arr);
  }
}
testStoreGeneric();
