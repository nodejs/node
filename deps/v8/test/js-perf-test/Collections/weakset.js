// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var SetBenchmark = new BenchmarkSuite('WeakSet', [1000], [
  new Benchmark('Add', false, false, 0, WeakSetAdd, WeakSetSetupBase,
      WeakSetTearDown),
  new Benchmark('Has', false, false, 0, WeakSetHas, WeakSetSetup,
      WeakSetTearDown),
  new Benchmark('Delete', false, false, 0, WeakSetDelete, WeakSetSetup,
      WeakSetTearDown),
]);


var ws;


function WeakSetSetupBase() {
  SetupObjectKeys();
  ws = new WeakSet;
}


function WeakSetSetup() {
  WeakSetSetupBase();
  WeakSetAdd();
}


function WeakSetTearDown() {
  ws = null;
}


function WeakSetAdd() {
  for (var i = 0; i < N; i++) {
    ws.add(keys[i]);
  }
}


function WeakSetHas() {
  for (var i = 0; i < N; i++) {
    if (!ws.has(keys[i])) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (ws.has(keys[i])) {
      throw new Error();
    }
  }
}


function WeakSetDelete() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    ws.delete(keys[i]);
  }
}
