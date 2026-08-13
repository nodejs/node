// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// The inlined sort snapshots the receiver's elements, runs comparefn, then
// copies the snapshot back specialized on the receiver's elements kind. With
// polymorphic {PACKED_SMI,PACKED} feedback that kind is the union
// PACKED_ELEMENTS, which no longer describes a receiver that comparefn has
// narrowed back to PACKED_SMI_ELEMENTS.

function sort(a) {
  function compare() {
    a.fill(0);
    return 0;
  }
  return a.sort(compare);
}

%PrepareFunctionForOptimization(sort);
for (let i = 0; i < 100; ++i) {
  sort([1, 2]);
  sort([{}, {}]);
}
%OptimizeMaglevOnNextCall(sort);
sort([1, 2]);

const object = {};
const bad = [object, {}];
sort(bad);

// A Smi-elements array must never hold a HeapObject.
assertFalse(%HasSmiElements(bad) && bad[0] === object);

// Mixed-kind feedback is not inlined, but still sorts correctly.
function sortMixed(a) {
  return a.sort((x, y) => x - y);
}
%PrepareFunctionForOptimization(sortMixed);
for (let i = 0; i < 100; ++i) {
  sortMixed([2, 1]);
  sortMixed([{}, {}]);
}
%OptimizeMaglevOnNextCall(sortMixed);
assertEquals([1, 2, 3], sortMixed([3, 1, 2]));

// Same via a Smi/double mix, whose union kind PACKED_DOUBLE_ELEMENTS is
// rejected by the double bailout rather than the kind-agreement one.
function sortDouble(a) {
  function compare(x, y) {
    a.fill(0);
    return x - y;
  }
  return a.sort(compare);
}
%PrepareFunctionForOptimization(sortDouble);
for (let i = 0; i < 100; ++i) {
  sortDouble([2, 1]);
  sortDouble([2.5, 1.5]);
}
%OptimizeMaglevOnNextCall(sortDouble);
sortDouble([2, 1]);
// The sort writes its snapshot back, so comparefn's fill is not observable.
assertEquals([1.5, 2.5], sortDouble([2.5, 1.5]));

// Sites whose receiver kinds agree keep the inlined path, including when the
// maps themselves differ.
function withProperty() {
  const a = [{}, {}];
  a.foo = 1;
  return a;
}
function sortSameKind(a) {
  return a.sort((x, y) => 0);
}
%PrepareFunctionForOptimization(sortSameKind);
for (let i = 0; i < 100; ++i) {
  sortSameKind([{}, {}]);
  sortSameKind(withProperty());
}
%OptimizeMaglevOnNextCall(sortSameKind);
sortSameKind([{}, {}]);
assertOptimized(sortSameKind);
for (let i = 0; i < 100; ++i) {
  sortSameKind([{}, {}]);
  sortSameKind(withProperty());
}
assertOptimized(sortSameKind);
