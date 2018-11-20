// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests that we don\'t hold promises.');

(async function test() {
  Protocol.Runtime.enable();
  Protocol.Debugger.enable();
  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});
  Protocol.HeapProfiler.enable();
  // Force inspector internal scripts compilation.
  await Protocol.Runtime.evaluate({expression: ''});
  let snapshot = '';
  Protocol.HeapProfiler.onAddHeapSnapshotChunk(msg => snapshot += msg.params.chunk);
  await Protocol.HeapProfiler.collectGarbage();
  await Protocol.HeapProfiler.takeHeapSnapshot();
  let initial_node_count = JSON.parse(snapshot).snapshot.node_count;

  await Protocol.Runtime.evaluate({
    expression: `for (let i = 0; i < ${initial_node_count / 4}; ++i) Promise.resolve()`});
  snapshot = '';
  Protocol.HeapProfiler.onAddHeapSnapshotChunk(msg => snapshot += msg.params.chunk);
  await Protocol.HeapProfiler.collectGarbage();
  await Protocol.HeapProfiler.takeHeapSnapshot();
  let without_storing_node_count = JSON.parse(snapshot).snapshot.node_count;
  let diff_without_storing = (without_storing_node_count - initial_node_count);

  if (diff_without_storing < initial_node_count / 4) {
    InspectorTest.log('SUCCESS');
  } else {
    InspectorTest.log('FAILED: looks like all promises were not collected.');
  }
  InspectorTest.completeTest();
})();
