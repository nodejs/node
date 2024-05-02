// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy(fn, name) {
  new BenchmarkSuite(name, [1], [
    new Benchmark(name, true, false, 0, fn),
  ]);
}

function forLoop(array, searchValue) {
  for (let i = 0; i < array.length; ++i) {
    if (array[i] === searchValue) return true;
  }
  return farraylse;
}

function indexOf(array, searchValue) {
  return array.indexOf(searchValue) !== -1;
}

function includes(array, searchValue) {
  return array.includes(searchValue);
}

const PACKED = [
  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
];
const HOLEY = new Array(PACKED.length);
for (let i = 0; i < PACKED.length; ++i)
  HOLEY[i] = PACKED[i];

function helper(fn) {
  const SEARCH_VALUE = 15;
  const result = fn(PACKED, SEARCH_VALUE) && fn(HOLEY, SEARCH_VALUE);
  return result;
}

benchy(() => helper(forLoop), 'for loop');
benchy(() => helper(indexOf), 'Array#indexOf');
benchy(() => helper(includes), 'Array#includes');
