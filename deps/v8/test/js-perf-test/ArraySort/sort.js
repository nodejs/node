// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(() => {

const size = 10;
let template_array = [];

for (let i = 0; i < size; ++i) {
  template_array[i] = Math.floor(Math.random() * size);
}

let packed_smi_array = Array.from(template_array);
let packed_double_array = Array.from(template_array, (x,_) => x + 0.1);
let packed_object_array = Array.from(template_array, (x,_) => `value ${x}`);

let holey_smi_array = new Array(size);
let holey_double_array = new Array(size);
let holey_object_array = new Array(size);

for (let i = 0; i < size; i += 5) {
  const x = Math.floor(Math.random() * size);

  holey_smi_array[i] = x;
  holey_double_array[i] = x + 0.1;
  holey_object_array[i] = `value ${x}`;
}

let dictionary_array = Array.from(template_array);
dictionary_array[%MaxSmi()] = 42;

function assert(condition, message) {
  if (!condition) {
    throw Error(message);
  }
}

assert(%HasFastPackedElements(packed_smi_array) &&
       %HasSmiElements(packed_smi_array),
       "Element kind is not PACKED_SMI_ELEMENTS");
assert(%HasFastPackedElements(packed_double_array) &&
       %HasDoubleElements(packed_double_array),
       "Element kind is not PACKED_DOUBLE_ELEMENTS");
assert(%HasFastPackedElements(packed_object_array) &&
       %HasObjectElements(packed_object_array),
       "Element kind is not PACKED_ELEMENTS");
assert(%HasHoleyElements(holey_smi_array) &&
       %HasSmiElements(holey_smi_array),
       "Element kind is not HOLEY_SMI_ELEMENTS");
assert(%HasHoleyElements(holey_double_array) &&
       %HasDoubleElements(holey_double_array),
       "Element kind is not HOLEY_DOUBLE_ELEMENTS");
assert(%HasHoleyElements(holey_object_array) &&
       %HasObjectElements(holey_object_array),
       "Element kind is not HOLEY_ELEMENTS");
assert(%HasDictionaryElements(dictionary_array),
       "Element kind is not DICTIONARY_ELEMENTS");

function PackedSmiSort() {
  packed_smi_array.sort();
}

function PackedDoubleSort() {
  packed_double_array.sort();
}

function PackedElementSort() {
  packed_object_array.sort();
}

function HoleySmiSort() {
  holey_smi_array.sort();
}

function HoleyDoubleSort() {
  holey_double_array.sort();
}

function HoleyElementSort() {
  holey_object_array.sort();
}

function DictionarySort() {
  dictionary_array.sort();
}

benchy('PackedSmi', PackedSmiSort);
benchy('PackedDouble', PackedDoubleSort);
benchy('PackedElement', PackedElementSort);

benchy('HoleySmi', HoleySmiSort);
benchy('HoleyDouble', HoleyDoubleSort);
benchy('HoleyElement', HoleyElementSort);

benchy('Dictionary', DictionarySort);

})();
