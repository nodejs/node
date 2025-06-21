// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const buf = new SharedArrayBuffer(8, { "maxByteLength": 8 });

class obj extends Uint8ClampedArray {
  constructor() {
    super(buf);
  }
  defineProperty() {
    for (let i = 0; i < 3; i++) {
      super.length;
    }
    SharedArrayBuffer.__proto__ = this;
  }
}

function opt() {
  new obj().defineProperty();
}

%PrepareFunctionForOptimization(opt);
opt();
opt();
opt();
opt();
%OptimizeFunctionOnNextCall(opt)
opt();
