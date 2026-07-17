// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-async-hooks

let ah = async_hooks.createHook(
{
  init(asyncId, type) {
    if (type !== 'PROMISE') { return; }
    assertThrows('asyncIds.push(asyncId);');
  }
});
ah.enable();

async function foo() {
  let x = { toString() { return 'modules-skip-1.js' } };
  assertThrows('await import(x);');
}
foo();
