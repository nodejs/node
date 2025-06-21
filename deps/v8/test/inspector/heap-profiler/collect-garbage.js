// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-incremental-marking

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests collectGarbage.');

contextGroup.addScript(`
function createWeakRef() {
  globalThis.weak_ref = new WeakRef(new Array(1000).fill(0));
}
function getWeakRef() {
  if (!globalThis.weak_ref.deref()) return 'WeakRef is cleared after GC.';
  return 'WeakRef is not cleared. GC did not happen?'
}
//# sourceURL=test.js`);

Protocol.Debugger.enable();
Protocol.HeapProfiler.enable();

InspectorTest.runAsyncTestSuite([
  async function testCollectGarbage() {
    await Protocol.Runtime.evaluate({ expression: 'createWeakRef()' });
    await Protocol.HeapProfiler.collectGarbage();
    let weak_ref = await Protocol.Runtime.evaluate({ expression: 'getWeakRef()' });
    InspectorTest.log(`WeakRef state: ${weak_ref.result.result.value}`);
  }
]);
