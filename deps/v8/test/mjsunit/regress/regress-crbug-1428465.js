// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  const v2 = new Int32Array(0xFFFF);
  this.WebAssembly.Memory.apply(this, v2);
} catch(e) {
}
