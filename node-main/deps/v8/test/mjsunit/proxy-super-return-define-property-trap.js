// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation
Proxy.prototype = Object.prototype;

let logs = [];

class Z extends Proxy {
  constructor() {
    super({}, {
      set() {
        logs.push("set");
        return true;
      },
      defineProperty() {
        logs.push("defineProperty");
        return true;
      }
    })
  }
  a = 1;
}

new Z();
assertEquals(["defineProperty"], logs);

logs = [];
new Z();
assertEquals(["defineProperty"], logs);
