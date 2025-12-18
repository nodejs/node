// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var MapBenchmark = new BenchmarkSuite('WeakMap', [1000], [
  new Benchmark('Set', false, false, 0, WeakMapSet, WeakMapSetupBase,
      WeakMapTearDown),
  new Benchmark('Has', false, false, 0, WeakMapHas, WeakMapSetup,
      WeakMapTearDown),
  new Benchmark('Get', false, false, 0, WeakMapGet, WeakMapSetup,
      WeakMapTearDown),
  new Benchmark('Delete', false, false, 0, WeakMapDelete, WeakMapSetup,
      WeakMapTearDown),
]);

var MapBenchmark = new BenchmarkSuite('WeakMapSetGet-Large', [1e7], [
  new Benchmark('Set-Get', false, true, 1, WeakMapSetGetLarge,
                WeakMapSetupBaseLarge, WeakMapTearDown),
]);

var MapBenchmark = new BenchmarkSuite('WeakMapSet-Huge', [1e8], [
  new Benchmark('Set-Get', false, true, 1, WeakMapSetHuge,
                WeakMapSetupBaseLarge, WeakMapTearDown),
]);

var MapBenchmark = new BenchmarkSuite('WeakMap-Constructor', [1000], [
  new Benchmark('Constructor', false, false, 0, WeakMapConstructor, SetupObjectKeyValuePairs,
      WeakMapTearDown),
]);


var wm;


function WeakMapSetupBase() {
  SetupObjectKeys();
  wm = new WeakMap;
}


function WeakMapSetupBaseLarge() {
  SetupObjectKeys(2 * LargeN);
  wm = new WeakMap;
}


function WeakMapSetup() {
  WeakMapSetupBase();
  WeakMapSet();
}


function WeakMapTearDown() {
  wm = null;
}


function WeakMapConstructor() {
  wm = new WeakMap(keyValuePairs);
}

function WeakMapSet() {
  for (var i = 0; i < N; i++) {
    wm.set(keys[i], i);
  }
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
  // more than once. Therefore, we do not check the return value of delete.
  for (var i = 0; i < N; i++) {
    wm.delete(keys[i]);
  }
}

function WeakMapSetGetLarge() {
  for (var i = 0; i < LargeN; i++) {
    wm.set(keys[i * 2], i);
  }
  for (var i = 0; i < LargeN; i++) {
    if (wm.get(keys[i * 2]) !== i) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * LargeN; i++) {
    if (wm.get(keys[i * 2 + 1]) !== undefined) {
      throw new Error();
    }
  }
}

function WeakMapSetHuge() {
  function Foo(i) {
    this.x = i;
  }
  let obj;
  for (let i = 0; i < LargeN; i++) {
    obj = new Foo(i);         // Make sure we do not scalar-replace the object.
    wm.set(obj, 1);
  }
}
