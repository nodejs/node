// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

var FG = new FinalizationRegistry (function (iter) { globalThis.FRRan = true; });
{
  let obj = {};
  // obj is its own unregister token and becomes unreachable after this
  // block. If the unregister token is held strongly this test will not
  // terminate.
  FG.register(obj, 42, obj);
}
function tryAgain() {
  gc();
  if (globalThis.FRRan || FG.cleanupSome()) {
    return;
  }
  setTimeout(tryAgain, 0);
}
tryAgain();
