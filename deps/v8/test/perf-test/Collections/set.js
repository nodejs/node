// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var SetBenchmark = new BenchmarkSuite('Set', [1000], [
  new Benchmark('Add', false, false, 0, SetAdd),
  new Benchmark('Has', false, false, 0, SetHas, SetSetup, SetTearDown),
  new Benchmark('Delete', false, false, 0, SetDelete, SetSetup, SetTearDown),
  new Benchmark('ForEach', false, false, 0, SetForEach, SetSetup, SetTearDown),
]);


var set;
var N = 10;


function SetSetup() {
  set = new Set;
  for (var i = 0; i < N; i++) {
    set.add(i);
  }
}


function SetTearDown() {
  map = null;
}


function SetAdd() {
  SetSetup();
  SetTearDown();
}


function SetHas() {
  for (var i = 0; i < N; i++) {
    if (!set.has(i)) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (set.has(i)) {
      throw new Error();
    }
  }
}


function SetDelete() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    set.delete(i);
  }
}


function SetForEach() {
  set.forEach(function(v, k) {
    if (v !== k) {
      throw new Error();
    }
  });
}
