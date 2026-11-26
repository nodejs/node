// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Overflow the transition array on the initial PackedSmi array map so that
// defining an accessor on a fresh array creates a new detached root map.
for (let i = 0; i < 1536; i++) {
  let o = [1];
  o['p' + i] = 1;
}

const getter = () => 1;
const setter = (v) => {};

// Case 1: Adding a complement accessor directly on a root map.
let a = [1];
Object.defineProperty(a, 'x', {get: getter, configurable: true});
Object.defineProperty(a, 'x', {set: setter, configurable: true});
a.y = 1;
a[0] = 1.5;
assertEquals(getter, Object.getOwnPropertyDescriptor(a, 'x').get);
assertEquals(setter, Object.getOwnPropertyDescriptor(a, 'x').set);

// Case 2: Adding a complement accessor on a special transition map whose
// descriptors are all inherited from the root map.
let b = [1];
Object.defineProperty(b, 'x', {get: getter, configurable: true});
b[0] = 1.5;
Object.defineProperty(b, 'x', {set: setter, configurable: true});
b[5] = 1.5;
assertEquals(getter, Object.getOwnPropertyDescriptor(b, 'x').get);
assertEquals(setter, Object.getOwnPropertyDescriptor(b, 'x').set);
