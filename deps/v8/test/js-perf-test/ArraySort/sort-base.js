// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kArraySize = 4000;
let template_array = [];

for (let i = 0; i < kArraySize; ++i) {
  template_array[i] = Math.floor(Math.random() * kArraySize);
}

let array_to_sort = [];

function assert(condition, message) {
  if (!condition) {
    throw Error(message);
  }
}

function AssertPackedSmiElements() {
  assert(%HasFastPackedElements(array_to_sort) &&
         %HasSmiElements(array_to_sort),
         "Element kind is not PACKED_SMI_ELEMENTS");
}

function AssertPackedDoubleElements() {
  assert(%HasFastPackedElements(array_to_sort) &&
         %HasDoubleElements(array_to_sort),
         "Element kind is not PACKED_DOUBLE_ELEMENTS");
}

function AssertPackedObjectElements() {
  assert(%HasFastPackedElements(array_to_sort) &&
         %HasObjectElements(array_to_sort),
         "Element kind is not PACKED_ELEMENTS");
}

function AssertHoleySmiElements() {
  assert(%HasHoleyElements(array_to_sort) &&
         %HasSmiElements(array_to_sort),
         "Element kind is not HOLEY_SMI_ELEMENTS");
}

function AssertHoleyDoubleElements() {
  assert(%HasHoleyElements(array_to_sort) &&
         %HasDoubleElements(array_to_sort),
         "Element kind is not HOLEY_DOUBLE_ELEMENTS");
}

function AssertHoleyObjectElements() {
  assert(%HasHoleyElements(array_to_sort) &&
         %HasObjectElements(array_to_sort),
         "Element kind is not HOLEY_ELEMENTS");
}

function AssertDictionaryElements() {
  assert(%HasDictionaryElements(array_to_sort),
         "Element kind is not DICTIONARY_ELEMENTS");
}

function CreatePackedSmiArray() {
  array_to_sort = Array.from(template_array);
  AssertPackedSmiElements();
}

function CreatePackedDoubleArray() {
  array_to_sort = Array.from(template_array, (x,_) => x + 0.1);
  AssertPackedDoubleElements();
}

function CreatePackedObjectArray() {
  array_to_sort = Array.from(template_array, (x,_) => `value ${x}`);
  AssertPackedObjectElements();
}

function CreateHoleySmiArray() {
  array_to_sort = new Array(kArraySize);
  for (let i = 0; i < kArraySize; ++i) {
    array_to_sort[i] = template_array[i];
  }

  AssertHoleySmiElements();
}

function CreateHoleyDoubleArray() {
  array_to_sort = new Array(kArraySize);
  for (let i = 0; i < kArraySize; ++i) {
    array_to_sort[i] = template_array[i] + 0.1;
  }

  AssertHoleyDoubleElements();
}

function CreateHoleyObjectArray() {
  array_to_sort = new Array(kArraySize);
  for (let i = 0; i < kArraySize; ++i) {
    array_to_sort[i] = `value ${template_array[i]}`;
  }

  AssertHoleyObjectElements();
}

function CreateDictionaryArray() {
  array_to_sort = Array.from(template_array);
  array_to_sort[%MaxSmi()] = 42;

  AssertDictionaryElements();
}

function Sort() {
  array_to_sort.sort();
}

function CreateSortFn(comparefns = []) {
  return () => {
    for (let cmpfn of comparefns) {
      array_to_sort.sort(cmpfn);
    }
  }
}

function cmp_smaller(a, b) {
  if (a < b) return -1;
  if (b < a) return 1;
  return 0;
}

function cmp_greater(a, b) { return cmp_smaller(b, a); }
