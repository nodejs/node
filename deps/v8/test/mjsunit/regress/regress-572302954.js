// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const props = {};
for (let i = 0; i < 31; i++) {
  props['p' + i] = {value: i, writable: true, configurable: true};
}

// Fill the 32-descriptor (length + p0..p30) JSArray map's TransitionArray up to
// kMaxNumberOfTransitions (1024 + 512 = 1536).
const keep = [];
for (let i = 0; i < 1536; i++) {
  const arr = Object.defineProperties([], props);
  Object.defineProperty(
      arr, 'b' + i, {value: 0, writable: true, configurable: true});
  keep.push(arr);
}

// TransitionArray is full, so adding `p31` creates a 33-descriptor
// (> kMaxElementsForLinearSearch = 32) JSArray root map M0 with 0 slack
// (GetBackPointer() == undefined, !IsDetached(), owns_descriptors == true).
const a = Object.defineProperties([], props);
a.p31 = 31;

// 1. Elements transition calls Map::CopyForElementsTransition, creating child
//    map(s) with backpointer chain -> M0 sharing M0's 33-descriptor array D0.
a.push(1.5);

// 2. Adding `extra` calls Map::CopyAddDescriptor on a special-transition map
//    that owns D0; it must not share/replace D0 via EnsureDescriptorSlack.
a.extra = 42;

// 3. Prototype transition walks FindRootMap() back to M0 and copies M0's
//    DescriptorArray D0.
Object.setPrototypeOf(a, {});
assertEquals(15, a.p15);
assertEquals(31, a.p31);
assertEquals(42, a.extra);

// Also test Map::ReplaceDescriptors when split_map in MapUpdater is an
// elements-transition map.
const a2 = Object.defineProperties([], props);
a2.p31 = 31;
a2.push(1.5);
a2.extra = 42;
Object.defineProperty(a2, 'extra', {value: 42, enumerable: false});
Object.setPrototypeOf(a2, {});
assertEquals(15, a2.p15);
assertEquals(31, a2.p31);
assertEquals(42, a2.extra);
