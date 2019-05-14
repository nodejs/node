// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax --expose-gc


new BenchmarkSuite('ManyClosures', [1000], [
  new Benchmark('CreateManyClosures', false, true, 1, CreateManyClosures,
                CreateManyClosures_Setup)
]);

// ----------------------------------------------------------------------------

// This program creates many closures and then allocates many arrays in order
// to trigger garbage collection cycles. The goal of this micro-benchmark is to
// evaluate the overhead of keeping the weak-list of optimized JS functions.
//
// c.f. https://bugs.chromium.org/p/v8/issues/detail?id=6637#c5


var a = [];
%NeverOptimizeFunction(CreateManyClosures_Setup);
function CreateManyClosures_Setup() {
  function g() {
    return (i) => i + 1;
  }

  // Create a closure and optimize.
  var f = g();

  f(0);
  f(0);
  %OptimizeFunctionOnNextCall(f);
  f(0);
  // Create 2M closures, those will get the optimized code.
  a = [];
  for (var i = 0; i < 2000000; i++) {
    var f = g();
    f();
    a.push(f);
  }
}

%NeverOptimizeFunction(CreateManyClosures);
function CreateManyClosures() {
  // Now cause scavenges.
  for (var i = 0; i < 50; i++) {
    gc(true);
  }
}
