// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('ForOf', [1000], [
  new Benchmark('ArrayValues', false, false, 0,
                ForOf, ForOfArraySetup, ForOfTearDown),
  new Benchmark('ArrayKeys', false, false, 0,
                ForOf, ForOfArrayKeysSetup, ForOfTearDown),
  new Benchmark('ArrayEntries', false, false, 0,
                ForOf, ForOfArrayEntriesSetup, ForOfTearDown),
  new Benchmark('Uint8Array', false, false, 0,
                ForOf, ForOfUint8ArraySetup, ForOfTearDown),
  new Benchmark('Float64Array', false, false, 0,
                ForOf, ForOfFloat64ArraySetup, ForOfTearDown),
  new Benchmark('String', false, false, 0,
                ForOf, ForOfStringSetup, ForOfTearDown),
]);


var iterable;
var N = 100;
var expected, result;


function ForOfArraySetupHelper(constructor) {
  iterable = new constructor(N);
  for (var i = 0; i < N; i++) iterable[i] = i;
  expected = N - 1;
}


function ForOfArraySetup() {
  ForOfArraySetupHelper(Array);
  // Default iterator is values().
}


function ForOfArrayKeysSetup() {
  ForOfArraySetupHelper(Array);
  iterable = iterable.keys();
}


function ForOfArrayEntriesSetup() {
  ForOfArraySetupHelper(Array);
  iterable = iterable.entries();
  expected = [N-1, N-1];
}


function ForOfUint8ArraySetup() {
  ForOfArraySetupHelper(Uint8Array);
}


function ForOfFloat64ArraySetup() {
  ForOfArraySetupHelper(Float64Array);
}


function ForOfStringSetup() {
  iterable = "abcdefhijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ0123456789";
  expected = "9";
}


function Equals(expected, actual) {
  if (expected === actual) return true;
  if (typeof expected !== typeof actual) return false;
  if (typeof expected !== 'object') return false;
  for (var k of Object.keys(expected)) {
    if (!(k in actual)) return false;
    if (!Equals(expected[k], actual[k])) return false;
  }
  for (var k of Object.keys(actual)) {
    if (!(k in expected)) return false;
  }
  return true;
}

function ForOfTearDown() {
  iterable = null;
  if (!Equals(expected, result)) {
    throw new Error("Bad result: " + result);
  }
}


function ForOf() {
  for (var x of iterable) {
    result = x;
  }
}
