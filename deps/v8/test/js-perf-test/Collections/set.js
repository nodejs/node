// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var SetSmiBenchmark = new BenchmarkSuite('Set-Smi', [1000], [
  new Benchmark('Set', false, false, 0, SetAddSmi, SetSetupSmiBase, SetTearDown),
  new Benchmark('Has', false, false, 0, SetHasSmi, SetSetupSmi, SetTearDown),
  new Benchmark('Delete', false, false, 0, SetDeleteSmi, SetSetupSmi, SetTearDown),
]);


var SetStringBenchmark = new BenchmarkSuite('Set-String', [1000], [
  new Benchmark('Set', false, false, 0, SetAddString, SetSetupStringBase, SetTearDown),
  new Benchmark('Has', false, false, 0, SetHasString, SetSetupString, SetTearDown),
  new Benchmark('Delete', false, false, 0, SetDeleteString, SetSetupString, SetTearDown),
]);


var SetObjectBenchmark = new BenchmarkSuite('Set-Object', [1000], [
  new Benchmark('Set', false, false, 0, SetAddObject, SetSetupObjectBase, SetTearDown),
  new Benchmark('Has', false, false, 0, SetHasObject, SetSetupObject, SetTearDown),
  new Benchmark('Delete', false, false, 0, SetDeleteObject, SetSetupObject, SetTearDown),
]);


var SetDoubleBenchmark = new BenchmarkSuite('Set-Double', [1000], [
  new Benchmark('Set', false, false, 0, SetAddDouble, SetSetupDoubleBase, SetTearDown),
  new Benchmark('Has', false, false, 0, SetHasDouble, SetSetupDouble, SetTearDown),
  new Benchmark('Delete', false, false, 0, SetDeleteDouble, SetSetupDouble, SetTearDown),
]);


var SetIterationBenchmark = new BenchmarkSuite('Set-Iteration', [1000], [
  new Benchmark('ForEach', false, false, 0, SetForEach, SetSetupSmi, SetTearDown),
]);


var SetIterationBenchmark = new BenchmarkSuite('Set-Iterator', [1000], [
  new Benchmark('Iterator', false, false, 0, SetIterator, SetSetupSmi, SetTearDown),
]);


var SetConstructorBenchmark = new BenchmarkSuite('Set-Constructor', [1000], [
  new Benchmark('Smi', false, false, 0, SetConstructorSmi, SetupSmiKeys, SetTearDown),
  new Benchmark('String', false, false, 0, SetConstructorString, SetupStringKeys, SetTearDown),
  new Benchmark('Object', false, false, 0, SetConstructorObject, SetupObjectKeys, SetTearDown),
  new Benchmark('Double', false, false, 0, SetConstructorDouble, SetupDoubleKeys, SetTearDown),
]);

var set;


function SetSetupSmiBase() {
  SetupSmiKeys();
  set = new Set;
}


function SetSetupSmi() {
  SetSetupSmiBase();
  SetAddSmi();
}


function SetSetupStringBase() {
  SetupStringKeys();
  set = new Set;
}


function SetSetupString() {
  SetSetupStringBase();
  SetAddString();
}


function SetSetupObjectBase() {
  SetupObjectKeys();
  set = new Set;
}


function SetSetupObject() {
  SetSetupObjectBase();
  SetAddObject();
}


function SetSetupDoubleBase() {
  SetupDoubleKeys();
  set = new Set;
}


function SetSetupDouble() {
  SetSetupDoubleBase();
  SetAddDouble();
}


function SetTearDown() {
  set = null;
}


function SetConstructorSmi() {
  set = new Set(keys);
}


function SetAddSmi() {
  for (var i = 0; i < N; i++) {
    set.add(keys[i], i);
  }
}


function SetHasSmi() {
  for (var i = 0; i < N; i++) {
    if (!set.has(keys[i])) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (set.has(keys[i])) {
      throw new Error();
    }
  }
}


function SetDeleteSmi() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    set.delete(keys[i]);
  }
}


function SetConstructorString() {
  set = new Set(keys);
}


function SetAddString() {
  for (var i = 0; i < N; i++) {
    set.add(keys[i], i);
  }
}


function SetHasString() {
  for (var i = 0; i < N; i++) {
    if (!set.has(keys[i])) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (set.has(keys[i])) {
      throw new Error();
    }
  }
}


function SetDeleteString() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    set.delete(keys[i]);
  }
}


function SetConstructorObject() {
  set = new Set(keys);
}


function SetAddObject() {
  for (var i = 0; i < N; i++) {
    set.add(keys[i], i);
  }
}


function SetHasObject() {
  for (var i = 0; i < N; i++) {
    if (!set.has(keys[i])) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (set.has(keys[i])) {
      throw new Error();
    }
  }
}


function SetDeleteObject() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    set.delete(keys[i]);
  }
}


function SetConstructorDouble() {
  set = new Set(keys);
}


function SetAddDouble() {
  for (var i = 0; i < N; i++) {
    set.add(keys[i], i);
  }
}


function SetHasDouble() {
  for (var i = 0; i < N; i++) {
    if (!set.has(keys[i])) {
      throw new Error();
    }
  }
  for (var i = N; i < 2 * N; i++) {
    if (set.has(keys[i])) {
      throw new Error();
    }
  }
}


function SetDeleteDouble() {
  // This is run more than once per setup so we will end up deleting items
  // more than once. Therefore, we do not the return value of delete.
  for (var i = 0; i < N; i++) {
    set.delete(keys[i]);
  }
}


function SetForEach() {
  set.forEach(function(v, k) {
    if (v !== k) {
      throw new Error();
    }
  });
}


function SetIterator() {
  var result = 0;
  for (const v of set) result += v;
  return result;
}
