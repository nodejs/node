// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

const kArraySize = 1000;
const kQuarterSize = kArraySize / 4;

let array = [];

// Copy a quarter of the elements from the middle to the front.
function CopyWithin() {
  return new Function(
    'array.copyWithin(0, kQuarterSize * 2, kQuarterSize * 3);');
}

createSuite('SmiCopyWithin', 1000, CopyWithin, SmiCopyWithinSetup);
createSuite('StringCopyWithin', 1000, CopyWithin, StringCopyWithinSetup);
createSuite('SparseSmiCopyWithin', 1000, CopyWithin, SparseSmiCopyWithinSetup);
createSuite(
    'SparseStringCopyWithin', 1000, CopyWithin, SparseStringCopyWithinSetup);

function SmiCopyWithinSetup() {
  array = [];
  for (let i = 0; i < kArraySize; ++i) array[i] = i;
}

function StringCopyWithinSetup() {
  array = [];
  for (let i = 0; i < kArraySize; ++i) array[i] = `Item no. ${i}`;
}

function SparseSmiCopyWithinSetup() {
  array = [];
  for (let i = 0; i < kArraySize; i += 10) array[i] = i;
}

function SparseStringCopyWithinSetup() {
  array = [];
  for (let i = 0; i < kArraySize; i += 10) array[i] = `Item no. ${i}`;
}

})();
