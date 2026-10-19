// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const read32 = address => memory.getUint32(address, true);
const write32 = (address, value) => memory.setUint32(address, value, true);

function materialize(x) {
  const object = {first: 34, second: 43};
  object.first = 2;
  object.second = 12;
  x++;
  return object === null;
}
const sample = {first: 13, second: 46};
%PrepareFunctionForOptimization(materialize);
for (let i = 0; i < 10; ++i) materialize(1);

const map = read32(Sandbox.getAddressOf(sample)) - 1;
const descriptors = read32(map + 24) - 1;
const first = read32(descriptors + 20);
const second = read32(descriptors + 32);
// Capture three fields, with the two stores occupying the object header.
write32(descriptors + 20, first - (2 << 20));
write32(descriptors + 32, second - (2 << 20));
%OptimizeFunctionOnNextCall(materialize);
materialize(2);
write32(descriptors + 20, first);
write32(descriptors + 32, second);

// The captured object has one element field, but claims two elements.
memory.setUint16(map + 8,
    Sandbox.getInstanceTypeIdFor('FIXED_DOUBLE_ARRAY_TYPE'), true);
materialize(1.5);
assertUnreachable();
