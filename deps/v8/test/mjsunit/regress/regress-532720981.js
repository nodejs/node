// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

Object.setPrototypeOf(globalThis, new Proxy({}, {
  get(target, prop) {
    if (prop === 'receive') {
      throw new Error("Triggering error crash during send()!");
    }
    return Reflect.get(target, prop);
  }
}));

try {
  send("test message");
} catch (error) {

}
