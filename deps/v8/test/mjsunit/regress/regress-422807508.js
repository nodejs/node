// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function ignore(){}

// We'll call outer_array.forEach(Array.prototype.forEach), which means the
// callback argument of the inner forEach will be the element(s) of the
// outer_array. Make sure these elements are callable.
let outer_array = [ignore];

function foo(a) {
    // The inner array has to have holey elements so that it is stable,
    // otherwise we'll lose the map information we have for it at the
    // start of the forEach loop.
    let inner_array = [null, /*hole*/, null];

    // Load from inner_array so that we have its map.
    inner_array[0];

    // Call forEach directly from another forEach, with no intermediate
    // function, so that their continuation builtins are next to each other.
    // inner_array is used so that the inner forEach iterates over a known
    // array.
    a.forEach(Array.prototype.forEach, inner_array);
}

%PrepareFunctionForOptimization(foo);
foo(outer_array);
foo(outer_array);
%OptimizeMaglevOnNextCall(foo);
foo(outer_array);
