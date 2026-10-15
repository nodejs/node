// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

let leakedDebug;
let sideEffectTriggered = false;

Object.setPrototypeOf(globalThis, new Proxy({}, {
  has(target, prop) {
    if (prop === 'queryObjects') {
      leakedDebug = globalThis.debug;
      sideEffectTriggered = true;
    }
    return false;
  }
}));

send(JSON.stringify({
  id: 1,
  method: 'Runtime.evaluate',
  params: {
    includeCommandLineAPI: true,
    throwOnSideEffect: true,
    expression: '1 + 1'
  }
}));

if (sideEffectTriggered || typeof leakedDebug === 'function') {
  throw new Error("Side effect triggered or API leaked during throwOnSideEffect");
}
