// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  new Uint32Array(new ArrayBuffer(1), 2, 3);
} catch (e) {
  assertEquals("start offset of Uint32Array should be a multiple of 4",
               e.message);
}
