// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof WebAssembly != 'undefined') {
  const memory = new WebAssembly.Memory({
      "initial": 1,
      "maximum": 10,
      "shared": true,
  });
  assertEquals(65536, memory.buffer.maxByteLength);
}
