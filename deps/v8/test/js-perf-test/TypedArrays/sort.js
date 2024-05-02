// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('base.js');

function CreateBenchmarks(constructors, comparefns = []) {
  var benchmarks = [];
  for (let constructor of constructors) {
    benchmarks.push(new Benchmark('Sort' + constructor.name, false, false, 0,
                                  CreateSortFn(comparefns),
                                  CreateSetupFn(constructor.ctor), TearDown));
  }
  return benchmarks;
}

const kArraySize = 3000;
const initialLargeArray = new Array(kArraySize);
for (let i = 0; i < kArraySize; ++i) {
  initialLargeArray[i] = Math.random() * kArraySize;
}

let array_to_sort = [];

function CreateSetupFn(constructor) {
  return () => {
    if (constructor == BigUint64Array || constructor == BigInt64Array) {
      array_to_sort = constructor.from(initialLargeArray,
                                       x => BigInt(Math.floor(x)));
    } else {
      array_to_sort = new constructor(initialLargeArray);
    }
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
