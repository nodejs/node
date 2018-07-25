// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('sort-base.js');

// Creates a comparison function, that will call the provided transform function
// after a set amount of comparisons. The transform function should cause the
// element kind of the array to change.
function CreateCompareFn(transformfn) {
  let counter = 0;
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

benchy('PackedSmiToPackedDouble', CreateSortFn([cmp_packed_smi_to_double]),
       CreatePackedSmiArray, AssertPackedDoubleElements);
benchy('HoleySmiToHoleyDouble', CreateSortFn([cmp_holey_smi_to_double]),
       CreateHoleySmiArray, AssertHoleyDoubleElements);
benchy('PackedDoubleToHoleyDouble', CreateSortFn([cmp_double_to_double]),
       CreatePackedDoubleArray, AssertHoleyDoubleElements);

let cmp_packed_to_dict = CreateCompareFn(() => array_to_sort[%MaxSmi()] = 42);
let cmp_holey_to_dict = CreateCompareFn(() => array_to_sort[%MaxSmi()] = 42);

benchy('PackedElementToDictionary', CreateSortFn([cmp_packed_to_dict]),
       CreatePackedObjectArray, AssertDictionaryElements);
benchy('HoleyElementToDictionary', CreateSortFn([cmp_holey_to_dict]),
       CreateHoleyObjectArray, AssertDictionaryElements);
