// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var MapSmiBenchmark = new BenchmarkSuite('Map-Smi', [1000], [
  new Benchmark('Set', false, false, 0, MapSetSmi, MapSetupSmiBase, MapTearDown),
  new Benchmark('Has', false, false, 0, MapHasSmi, MapSetupSmi, MapTearDown),
  new Benchmark('Get', false, false, 0, MapGetSmi, MapSetupSmi, MapTearDown),
  new Benchmark('Delete', false, false, 0, MapDeleteSmi, MapSetupSmi, MapTearDown),
]);


var MapStringBenchmark = new BenchmarkSuite('Map-String', [1000], [
  new Benchmark('Set', false, false, 0, MapSetString, MapSetupStringBase, MapTearDown),
  new Benchmark('Has', false, false, 0, MapHasString, MapSetupString, MapTearDown),
  new Benchmark('Get', false, false, 0, MapGetString, MapSetupString, MapTearDown),
  new Benchmark('Delete', false, false, 0, MapDeleteString, MapSetupString, MapTearDown),
]);


var MapObjectBenchmark = new BenchmarkSuite('Map-Object', [1000], [
  new Benchmark('Set', false, false, 0, MapSetObject, MapSetupObjectBase, MapTearDown),
  new Benchmark('Has', false, false, 0, MapHasObject, MapSetupObject, MapTearDown),
  new Benchmark('Get', false, false, 0, MapGetObject, MapSetupObject, MapTearDown),
  new Benchmark('Delete', false, false, 0, MapDeleteObject, MapSetupObject, MapTearDown),
]);


var MapIterationBenchmark = new BenchmarkSuite('Map-Iteration', [1000], [
  new Benchmark('ForEach', false, false, 0, MapForEach, MapSetupSmi, MapTearDown),
]);


var map;


function MapSetupSmiBase() {
  SetupSmiKeys();
  map = new Map;
}


function MapSetupSmi() {
  MapSetupSmiBase();
  MapSetSmi();
}


function MapSetupStringBase() {
  SetupStringKeys();
  map = new Map;
}


function MapSetupString() {
  MapSetupStringBase();
  MapSetString();
}


function MapSetupObjectBase() {
  SetupObjectKeys();
  map = new Map;
}


function MapSetupObject() {
  MapSetupObjectBase();
  MapSetObject();
}


function MapTearDown() {
  map = null;
}


function MapSetSmi() {
  for (var i = 0; i < N; i++) {
    map.set(keys[i], i);
  }
}


function MapHasSmi() {
  for (var i = 0; i < N; i++) {
    if (!map.has(keys[i])) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (map.has(keys[i])) {
      throw new Error();
    }
  }
}


function MapGetSmi() {
  for (var i = 0; i < N; i++) {
    if (map.get(keys[i]) !== i) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (map.get(keys[i]) !== undefined) {
      throw new Error();
    }
  }
}


function MapDeleteSmi() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    map.delete(keys[i]);
  }
}


function MapSetString() {
  for (var i = 0; i < N; i++) {
    map.set(keys[i], i);
  }
}


function MapHasString() {
  for (var i = 0; i < N; i++) {
    if (!map.has(keys[i])) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (map.has(keys[i])) {
      throw new Error();
    }
  }
}


function MapGetString() {
  for (var i = 0; i < N; i++) {
    if (map.get(keys[i]) !== i) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (map.get(keys[i]) !== undefined) {
      throw new Error();
    }
  }
}


function MapDeleteString() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    map.delete(keys[i]);
  }
}


function MapSetObject() {
  for (var i = 0; i < N; i++) {
    map.set(keys[i], i);
  }
}


function MapHasObject() {
  for (var i = 0; i < N; i++) {
    if (!map.has(keys[i])) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (map.has(keys[i])) {
      throw new Error();
    }
  }
}


function MapGetObject() {
  for (var i = 0; i < N; i++) {
    if (map.get(keys[i]) !== i) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (map.get(keys[i]) !== undefined) {
      throw new Error();
    }
  }
}


function MapDeleteObject() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    map.delete(keys[i]);
  }
}


function MapForEach() {
  map.forEach(function(v, k) {
    if (v !== k) {
      throw new Error();
    }
  });
}
