// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kPrefixOpcodes = {
  'GC': 0xfb,
  'Numeric': 0xfc,
  'Simd': 0xfd,
  'Atomic': 0xfe
}

function wrap(f) {
    return f();
}

const obj = wrap(() => ({
  2: 1701,
}));

for (let i = 0; i < 100; i++) {
  try {
    const data = d8.serializer.serialize(obj);
    const array = new Uint8Array(data);
    array[3] = i;
    d8.serializer.deserialize(data);
  } catch (e) {}
}
