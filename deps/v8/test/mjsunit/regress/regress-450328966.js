// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const err = new Error();
class B {
  m() {
    return super.stack;
  }
}

Object.setPrototypeOf(B.prototype, err);
const b = new B();
b.m.call(0x4141414 >> 1);
