// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var MapBenchmark = new BenchmarkSuite('WeakMap', [1000], [
  new Benchmark('Set', false, false, 0, WeakMapSet),
  new Benchmark('Has', false, false, 0, WeakMapHas, WeakMapSetup,
      WeakMapTearDown),
  new Benchmark('Get', false, false, 0, WeakMapGet, WeakMapSetup,
      WeakMapTearDown),
  new Benchmark('Delete', false, false, 0, WeakMapDelete, WeakMapSetup,
      WeakMapTearDown),
]);


var wm;
var N = 10;
var keys = [];


for (var i = 0; i < N * 2; i++) {
  keys[i] = {};
}


function WeakMapSetup() {
  wm = new WeakMap;
  for (var i = 0; i < N; i++) {
    wm.set(keys[i], i);
  }
}


function WeakMapTearDown() {
  wm = null;
}


function WeakMapSet() {
  WeakMapSetup();
  WeakMapTearDown();
}


function WeakMapHas() {
  for (var i = 0; i < N; i++) {
    if (!wm.has(keys[i])) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (wm.has(keys[i])) {
      throw new Error();
    }
  }
}


function WeakMapGet() {
  for (var i = 0; i < N; i++) {
    if (wm.get(keys[i]) !== i) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (wm.get(keys[i]) !== undefined) {
      throw new Error();
    }
  }
}


function WeakMapDelete() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    wm.delete(keys[i]);
  }
}
