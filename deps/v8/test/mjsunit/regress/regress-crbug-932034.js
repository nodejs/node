// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --mock-arraybuffer-allocator

// Verify on 32-bit architectures, a byte length overflow is handled gracefully.
try {
  new BigInt64Array(%MaxSmi());
} catch(e) {
  assertInstanceof(e, RangeError);
}
