// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-async-hooks --expose-gc

let v0 = [];
let v1 = 1;

let v2 = async_hooks.createHook({
  after: () => {
    async_hooks.createHook(v1);
    gc();
  }
});

v2.enable();

new Promise(v3 => v3(42)).then(() => { }).catch(() => { });
v1 = v0;
