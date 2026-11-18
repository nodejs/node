// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Build a dense fast-elements array at the maximum FixedArray capacity so that
// growing its length by 1 stays in fast elements (below kMaxGap) and overflows
// FixedDoubleArray::kMaxLength inside JSArray::SetLength.
const max_capacity = d8.constants.maxFixedArrayCapacity;
let full_array = [1.1];
while (full_array.length < max_capacity) {
  full_array = full_array.concat(full_array);
}
assertEquals(max_capacity, full_array.length);

// Sloppy and strict StoreIC paths through Accessors::ArrayLengthSetter.
function setLengthSloppy(arr, len) {
  arr.length = len;
}
function setLengthStrict(arr, len) {
  'use strict';
  arr.length = len;
}

// Warm up StoreICs with a valid array length update first.
const warmup_array = [1.1];
setLengthSloppy(warmup_array, 2);
setLengthStrict(warmup_array, 3);

assertThrows(() => setLengthSloppy(full_array, max_capacity + 1), RangeError);
assertEquals(max_capacity, full_array.length);

assertThrows(() => setLengthStrict(full_array, max_capacity + 1), RangeError);
assertEquals(max_capacity, full_array.length);

// Direct Object::SetPropertyWithAccessor and JSArray::ArraySetLength paths.
assertThrows(
    () => Reflect.set(full_array, 'length', max_capacity + 1), RangeError);
assertEquals(max_capacity, full_array.length);

assertThrows(
    () => Object.defineProperty(
        full_array, 'length', {value: max_capacity + 1}),
    RangeError);
assertEquals(max_capacity, full_array.length);

// Verify the array remains intact and usable after the failed length updates.
assertEquals(1.1, full_array[0]);
assertEquals(1.1, full_array[max_capacity - 1]);
full_array.length = 1;
assertEquals(1, full_array.length);
assertEquals(1.1, full_array[0]);
