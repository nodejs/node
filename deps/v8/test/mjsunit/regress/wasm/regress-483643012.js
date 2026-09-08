// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let __v_6 = new WebAssembly.Memory({
  initial: 1,
  maximum: 2,
  shared: true,
});
const __v_7 = __v_6.toFixedLengthBuffer();
assertEquals(65536, __v_7.byteLength);
const __v_9 = new ArrayBuffer(100, __v_7);
assertEquals(100, __v_9.byteLength);
