// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('sort-base.js');

function SortAsc() {
  array_to_sort.sort(cmp_smaller);
}

function Random(length) {
  array_to_sort = [];
  for (let i = 0; i < length; ++i) {
    array_to_sort.push(Math.floor(Math.random() * length));
  }
  AssertPackedSmiElements();
}

function Sorted(length) {
  array_to_sort = [];
  for (let i = 0; i < length; ++i) {
    array_to_sort.push(i);
  }
  AssertPackedSmiElements();
}

function CreateSortSuitesForLength(length) {
  createSortSuite('Random' + length, 1000, SortAsc, () => Random(length));
  createSortSuite('Sorted' + length, 1000, SortAsc, () => Sorted(length));
}

CreateSortSuitesForLength(10);
CreateSortSuitesForLength(100);
CreateSortSuitesForLength(1000);
CreateSortSuitesForLength(10000);
CreateSortSuitesForLength(100000);
