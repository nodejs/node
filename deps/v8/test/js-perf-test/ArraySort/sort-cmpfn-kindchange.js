// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('sort-base.js');

// Creates a comparison function, that will call the provided transform function
// after a set amount of comparisons. The transform function should cause the
// element kind of the array to change.
function CreateCompareFn(transformfn) {
  return (a, b) => {
    ++counter;
    if (counter == kArraySize/2) {
      transformfn();
    }

    return cmp_smaller(a, b);
  }
}

let cmp_packed_smi_to_double = CreateCompareFn(() => array_to_sort.push(0.1));
let cmp_holey_smi_to_double = CreateCompareFn(() => array_to_sort.push(0.1));
let cmp_double_to_double = CreateCompareFn(() => array_to_sort.length *= 2);

createSortSuite(
    'PackedSmiToPackedDouble', 1000, CreateSortFn([cmp_packed_smi_to_double]),
    CreatePackedSmiArray, AssertPackedDoubleElements);
createSortSuite(
    'HoleySmiToHoleyDouble', 1000, CreateSortFn([cmp_holey_smi_to_double]),
    CreateHoleySmiArray, AssertHoleyDoubleElements);
createSortSuite(
    'PackedDoubleToHoleyDouble', 1000, CreateSortFn([cmp_double_to_double]),
    CreatePackedDoubleArray, AssertHoleyDoubleElements);

let cmp_packed_to_dict = CreateCompareFn(() => array_to_sort[%MaxSmi()] = 42);
let cmp_holey_to_dict = CreateCompareFn(() => array_to_sort[%MaxSmi()] = 42);

createSortSuite(
    'PackedElementToDictionary', 1000, CreateSortFn([cmp_packed_to_dict]),
    CreatePackedObjectArray, AssertDictionaryElements);
createSortSuite(
    'HoleyElementToDictionary', 1000, CreateSortFn([cmp_holey_to_dict]),
    CreateHoleyObjectArray, AssertDictionaryElements);
