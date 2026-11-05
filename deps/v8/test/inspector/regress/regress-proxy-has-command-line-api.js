// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests that CommandLineAPIScope uses HasRealNamedProperty and does not trigger a Proxy has trap on globalThis.');

contextGroup.addScript(`
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
`);

(async function test() {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    includeCommandLineAPI: true,
    throwOnSideEffect: true,
    expression: '1 + 1',
  }));
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    expression:
        '({sideEffectTriggered, leakedDebugType: typeof leakedDebug, globalDebugType: typeof globalThis.debug})',
    returnByValue: true,
  }));
  InspectorTest.completeTest();
})();
