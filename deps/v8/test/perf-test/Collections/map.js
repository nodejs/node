// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var MapBenchmark = new BenchmarkSuite('Map', [1000], [
  new Benchmark('Set', false, false, 0, MapSet),
  new Benchmark('Has', false, false, 0, MapHas, MapSetup, MapTearDown),
  new Benchmark('Get', false, false, 0, MapGet, MapSetup, MapTearDown),
  new Benchmark('Delete', false, false, 0, MapDelete, MapSetup, MapTearDown),
  new Benchmark('ForEach', false, false, 0, MapForEach, MapSetup, MapTearDown),
]);


var map;
var N = 10;


function MapSetup() {
  map = new Map;
  for (var i = 0; i < N; i++) {
    map.set(i, i);
  }
}


function MapTearDown() {
  map = null;
}


function MapSet() {
  MapSetup();
  MapTearDown();
}


function MapHas() {
  for (var i = 0; i < N; i++) {
    if (!map.has(i)) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (map.has(i)) {
      throw new Error();
    }
  }
}


function MapGet() {
  for (var i = 0; i < N; i++) {
    if (map.get(i) !== i) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (map.get(i) !== undefined) {
      throw new Error();
    }
  }
}


function MapDelete() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    map.delete(i);
  }
}


function MapForEach() {
  map.forEach(function(v, k) {
    if (v !== k) {
      throw new Error();
    }
  });
}
