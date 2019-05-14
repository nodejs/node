// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let arr = [];
// Make the array large enough to trigger re-checking for compaction.
arr[1000] = 0x1234;

arr.__defineGetter__(256, function () {
    // Remove the getter so we can compact the array.
    delete arr[256];
    // Trigger compaction.
    arr.unshift(1.1);
});

let results = Object.entries(arr);
%HeapObjectVerify(results);
%HeapObjectVerify(arr);
let str = results.toString();
