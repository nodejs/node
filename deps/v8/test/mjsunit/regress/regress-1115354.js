// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-async-hooks

const sab = new SharedArrayBuffer(16);
const i32a = new Int32Array(sab);
const result = Atomics.waitAsync(i32a, 0, 0);

Atomics.notify(i32a, 0);
let hook = async_hooks.createHook(
{
  promiseResolve: function() { }
});
hook.enable();
