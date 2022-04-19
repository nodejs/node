// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-array-grouping

// Test OOB indexing on fast path.
let arr1 = [];
for (let i = 0; i < 32; i++) arr1.push(i);
let popped = false;
let grouped1 = arr1.groupBy(() => {
  // Pop all of the elements to trigger right-trimming of the elements
  // FixedArray.
  for (let i = 0, len = arr1.length; i < len; i++) {
    arr1.pop();
  }
});
// 'undefined' is the only group.
assertArrayEquals(['undefined'], Object.getOwnPropertyNames(grouped1));
// 0 the only value in the group because the grouping function pops the entire
// array.
let expectedGrouped1 = [0];
for (let i = 1; i < 32; i++) expectedGrouped1.push(undefined);
assertArrayEquals(expectedGrouped1, grouped1['undefined']);

// Test result ElementsKind deduction on fast path.
//
// Initial Smi array, but due to length truncation result is not a Smi array.
let arr2 = [0,1,2,3,4,5,6,7,8,9];
let grouped2 = arr2.groupBy(() => { arr2.length = 2; });
// 'undefined' is the only group.
assertArrayEquals(['undefined'], Object.getOwnPropertyNames(grouped2));
// 0,1 are the only values in the group because the source array gets truncated
// to length 2.
let expectedGrouped2 = [0,1];
for (let i = 2; i < 10; i++) expectedGrouped2.push(undefined);
assertArrayEquals(expectedGrouped2, grouped2['undefined']);
