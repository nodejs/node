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


var SetIterationBenchmark = new BenchmarkSuite('Set-Iteration', [1000], [
  new Benchmark('ForEach', false, false, 0, SetForEach, SetSetupSmi, SetTearDown),
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


function SetTearDown() {
  set = null;
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


function SetForEach() {
  set.forEach(function(v, k) {
    if (v !== k) {
      throw new Error();
    }
  });
}
