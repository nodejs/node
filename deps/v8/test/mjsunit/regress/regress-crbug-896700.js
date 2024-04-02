// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --async-stack-traces --expose-async-hooks

async_hooks.createHook({
  after() { throw new Error(); }
}).enable();
Promise.resolve().then();
