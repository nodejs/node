// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-async-hooks

const failing_proxy = new Proxy({}, new Proxy({}, {
  get() {
    throw "No trap should fire";
  }
}));

assertThrows(() => async_hooks.createHook(failing_proxy));
