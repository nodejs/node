// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-async-hooks

async function* foo() {
  await 1;
  throw new Error();
}

(async () => {
  for await (const x of foo()) { }
})();

async_hooks.createHook({
  promiseResolve() {
    throw new Error();
  }
}).enable()
