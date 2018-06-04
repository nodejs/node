// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let typedArrayIntConstructors = [
  {name: "Uint8", ctor: Uint8Array},
  {name: "Int8", ctor: Int8Array},
  {name: "Uint16", ctor: Uint16Array},
  {name: "Int16", ctor: Int16Array},
  {name: "Uint32", ctor: Uint32Array},
  {name: "Int32", ctor: Int32Array},
  {name: "Uint8Clamped", ctor: Uint8ClampedArray},
];

let typedArrayFloatConstructors = [
  {name: "Float32", ctor: Float32Array},
  {name: "Float64", ctor: Float64Array},
];

function CreateBenchmarks(constructors, comparefns = []) {
  var benchmarks = [];
  for (let constructor of constructors) {
    benchmarks.push(new Benchmark('Sort' + constructor.name, false, false, 0,
                                  CreateSortFn(comparefns),
                                  CreateSetupFn(constructor.ctor), TearDown));
  }
  return benchmarks;
}

const size = 3000;
const initialLargeArray = new Array(size);
for (let i = 0; i < size; ++i) {
  initialLargeArray[i] = Math.random() * 3000;
}

let array_to_sort = [];

function CreateSetupFn(constructor) {
  return () => {
    array_to_sort = new constructor(initialLargeArray);
  }
}

function CreateSortFn(comparefns) {
  if (comparefns.length == 0) {
      return () => array_to_sort.sort();
  }

  return () => {
    for (let cmpfn of comparefns) {
      array_to_sort.sort(cmpfn);
    }
  }
}

function TearDown() {
  array_to_sort = void 0;
}

function cmp_smaller(a, b) {
  if (a < b) return -1;
  if (b < a) return 1;
  return 0;
}

function cmp_greater(a, b) { cmp_smaller(b, a); }
