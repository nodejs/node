// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

const buffer = new ArrayBuffer(100);
const options = new Proxy({}, {
  get(target, property, receiver) {
    if (property === 'builtins') {
      buffer.transfer();
      gc();
    }
  }
});
WebAssembly.validate(buffer, options);
