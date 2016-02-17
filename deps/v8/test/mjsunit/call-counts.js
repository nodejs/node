// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noalways-opt

// We disable vector store ICs because slot indices change when this option
// is enabled.

// Locations in the type feedback vector where call counts are maintained for
// the two calls made from bar();

(function() {
  const kFooCallExtraIndex = 5;
  const kArrayCallExtraIndex = 7;

  function GetCallCount(func, slot) {
    var vector = %GetTypeFeedbackVector(func);
    // Call counts are recorded doubled.
    var value = %FixedArrayGet(vector, slot);
    return Math.floor(value / 2);
  }

  function foo(a) { return a[3] * 16; }

  function bar(a) {
    var result = 0;
    for (var i = 0; i < 10; i++) {
      result = foo(a);
      if (i % 2 === 0) {
        var r = Array();
        r[0] = 1;
        result += r[0];
      }
    }
    return result;
  }

  var a = [1, 2, 3];
  bar(a);
  assertEquals(10, GetCallCount(bar, kFooCallExtraIndex));
  assertEquals(5, GetCallCount(bar, kArrayCallExtraIndex));

  %OptimizeFunctionOnNextCall(bar);
  bar(a);
})();
