// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --no-enable-slow-asserts
//
// Disable slow asserts, because repeated array re-validation makes this
// test extremely slow.

arr = [];

// Transition the array to dictionary mode, with length longer than
// the FixedArray max length.
arr.length = d8.constants.maxFixedArrayCapacity + 1;
assertTrue(%HasDictionaryElements(arr));

assertThrows(()=> {
    arr.fill(0, {
        valueOf() {
            arr.length = 32;
            assertTrue(%HasDictionaryElements(arr));
            arr.fill(0);
            assertTrue(%HasFastElements(arr));
        }
    })
}, RangeError);
