// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// A CheckMaps that migrates a deprecated map in place (CheckMapsWithMigration
// and its AndDeopt sibling, used when no migration target is available) only
// updates the representation of the checked object's own fields; it must not
// invalidate unrelated cached maps or loaded properties.

function C(a) {
  this.a = a;
}

function make(n) {
  const arr = [];
  for (let i = 0; i < n; i++) arr.push(new C(i));
  return arr;
}

function sumAndDeprecate(arr, deprecateAt) {
  const helper = {x: 1, y: 2};
  let sum = 0;
  for (let i = 0; i < arr.length; i++) {
    if (i === deprecateAt) {
      // Storing a double into a field previously holding a Smi/tagged value
      // deprecates C's map (field representation generalization), which can
      // trigger a live-object migration on subsequent CheckMaps for other
      // C instances sharing the old map.
      arr[i].a = 1.5;
    }
    sum += arr[i].a;
    // helper's map/shape must remain known across the migration above.
    sum += helper.x + helper.y;
  }
  return sum;
}

%PrepareFunctionForOptimization(sumAndDeprecate);
const a1 = make(20);
const expected = sumAndDeprecate(a1, 10);

const a2 = make(20);
sumAndDeprecate(a2, -1);
%OptimizeFunctionOnNextCall(sumAndDeprecate);

const a3 = make(20);
assertEquals(expected, sumAndDeprecate(a3, 10));
const a4 = make(20);
assertEquals(expected, sumAndDeprecate(a4, 10));
