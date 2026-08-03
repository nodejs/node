// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

async function f1() {
  d8.debugger.enable();
  throw new Error();
}

async function f2() {
  try {
    await f1();
  } catch (_e) {
  }
}

(async () => {
  await f2();
  await f2();
})();

(async () => {
  d8.debugger.disable();
})();

gc();
