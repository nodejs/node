// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan

Object.defineProperty(Object.prototype, "test", {});

class Base {
  constructor() {
    return new Proxy(this, {
      defineProperty(target, key) {
        return true;
      }
    });
  }
}
let key = "test";
class Child extends Base {
  [key] = "basic";
}
let c = new Child();
c = new Child();
