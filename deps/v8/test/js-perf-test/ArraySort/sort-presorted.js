// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('sort-base.js');

const kLength = 2e4;
const kLengthHalf = kLength >>> 1;

function SortAsc() {
  array_to_sort.sort(cmp_smaller);
}

function Up(a, length) {
  for (let i = 0; i < length; ++i) {
    a.push(i);
  }
}

function Down(a, length) {
  for (let i = 0; i < length; ++i) {
    a.push(length - i);
  }
}

function SawSeq(a, tooth, length) {
  let count = 0;
  while (true) {
    for (let i = 0; i < tooth; ++i) {
      a.push(i);
      if (++count >= length) return;
    }
  }
}

function SawSeq2(a, tooth, length) {
  let count = 0;
  while (true) {
    for (let i = 0; i < tooth; ++i) {
      a.push(i);
      if (++count >= length) return;
    }
    for (let i = 0; i < tooth; ++i) {
      a.push(tooth - i);
      if (++count >= length) return;
    }
  }
}

function SawSeq3(a, tooth, length) {
  let count = 0;
  while (true) {
    for (let i = 0; i < tooth; ++i) {
      a.push(tooth - i);
      if (++count >= length) return;
    }
  }
}

function Random(a, length) {
  for (let i = 0; i < length; ++i) {
    a.push(Math.floor(Math.random() * length));
  }
}

function TearDown() {
  // Sanity check that the array is sorted.
  let length = array_to_sort.length - 1;
  for (let i = 0; i < length; ++i) {
    if (array_to_sort[i] > array_to_sort[i + 1]) {
      throw "Not sorted correctly: i = " + i;
    }
  }
  array_to_sort = [];
}

let SetupSaw1000 = () => SawSeq(array_to_sort, 1000, kLength);
let SetupSaw500 = () => SawSeq(array_to_sort, 500, kLength);
let SetupSaw200 = () => SawSeq(array_to_sort, 200, kLength);
let SetupSaw200Sym = () => SawSeq2(array_to_sort, 200, kLength);
let SetupSaw200Down = () => SawSeq3(array_to_sort, 200, kLength);

function SetupPreSortedHalfs(firstfn, secondfn) {
  array_to_sort = [];
  firstfn(array_to_sort, kLengthHalf);
  secondfn(array_to_sort, kLengthHalf);
}

let SetupUpDown = () => SetupPreSortedHalfs(Up, Down);
let SetupUpUp = () => SetupPreSortedHalfs(Up, Up);
let SetupDownDown = () => SetupPreSortedHalfs(Down, Down);
let SetupDownUp = () => SetupPreSortedHalfs(Down, Up);

createSortSuite(
    'Random', 1000, SortAsc, () => Random(array_to_sort, kLength), TearDown);
createSortSuite(
    'Up', 1000, SortAsc, () => Up(array_to_sort, kLength), TearDown);
createSortSuite(
    'Down', 1000, SortAsc, () => Down(array_to_sort, kLength), TearDown);
createSortSuite('Saw1000', 1000, SortAsc, SetupSaw1000, TearDown);
createSortSuite('Saw500', 1000, SortAsc, SetupSaw500, TearDown);
createSortSuite('Saw200', 1000, SortAsc, SetupSaw200, TearDown);
createSortSuite('Saw200Symmetric', 1000, SortAsc, SetupSaw200Sym, TearDown);
createSortSuite('Saw200Down', 1000, SortAsc, SetupSaw200Down, TearDown);
createSortSuite('UpDown', 1000, SortAsc, SetupUpDown, TearDown);
createSortSuite('UpUp', 1000, SortAsc, SetupUpUp, TearDown);
createSortSuite('DownDown', 1000, SortAsc, SetupDownDown, TearDown);
createSortSuite('DownUp', 1000, SortAsc, SetupDownUp, TearDown);
