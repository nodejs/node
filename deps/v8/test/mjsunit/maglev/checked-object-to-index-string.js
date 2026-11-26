// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// A string key that is not an array index is a property name, even when the
// array has seen out-of-bounds loads.

function load(array, key) {
  return array[key];
}

const array = [1, 2, 3];
array.foo = 7;
array['-1'] = 8;

%PrepareFunctionForOptimization(load);
assertEquals(2, load(array, '1'));
assertEquals(3, load(array, 2));
assertEquals(undefined, load(array, 5));
%OptimizeMaglevOnNextCall(load);
assertEquals(2, load(array, '1'));
assertEquals(7, load(array, 'foo'));
assertEquals(8, load(array, '-1'));
