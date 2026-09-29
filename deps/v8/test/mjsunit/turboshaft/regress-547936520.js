// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

// `pad` ensures `holder.y` is at in-object offset 16 to avoid alias
// collision with `object.target` at offset 12 in StoreStoreElimination.
const holder = { pad: null, y: null, prev: null };

function candidate() {
  let object;
  let sink = 0;
  let i = 0;
  while (true) {
    holder.prev = holder.y;
    object = { target: null };
    holder.y = object;
    if (i < 6) {
      object.target = 0x42;
      sink = sink ^ i ^ (sink << 1) ^ (sink << 2) ^ (sink << 3) ^ (sink << 4) ^
             (sink << 5) ^ (sink << 6) ^ (sink << 7) ^ (sink << 8) ^ (sink << 9) ^
             (sink << 10) ^ (sink << 11) ^ (sink << 12) ^ (sink << 13) ^
             (sink << 14) ^ (sink << 15);
      i = (i + 1) | 0;
    } else {
      break;
    }
  }
  object.target = 0x43;
  return [object, sink];
}

%PrepareFunctionForOptimization(candidate);
candidate();
candidate();

%OptimizeFunctionOnNextCall(candidate);
candidate();

assertEquals(0x42, holder.prev.target);
