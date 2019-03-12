// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const globalThis = this;
Object.setPrototypeOf(this, new Proxy({}, {
  get(target, prop, receiver) {
    assertTrue(receiver === globalThis);
  }
}));
undefined_name_access
