// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let arr = new Array(0x10000);
let resolve_element_closures = new Array(0x10000);

for (let i = 0; i < arr.length; i++) {
    arr[i] = new Promise(() => {});
    arr[i].then = ((idx, resolve) => {
        resolve_element_closures[idx] = resolve;
    }).bind(null, i);
}

Promise.all(arr);

// 0xffff is too large, transitions to DICTIONARY_ELEMENTS
resolve_element_closures[0xffff]();

// grows the capacity, the elements kind of the result array is still DICTIONARY_ELEMENTS, but the elements object of it is no more a dictionary.
resolve_element_closures[100]();

// You can observe that V8 crashes here in debug mode.
resolve_element_closures[0xfffe]();
