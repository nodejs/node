// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-incremental-marking

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/572276045.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));
contextGroup.addScript(`
const table = new WebAssembly.Table({ element: 'structref', initial: 1 });
`);

const snapshotChunks = [];

Protocol.HeapProfiler.onAddHeapSnapshotChunk(msg => {
  snapshotChunks.push(msg.params.chunk);
});

Protocol.HeapProfiler.takeHeapSnapshot().then(() => {
  const fullSnapshot = JSON.parse(snapshotChunks.join(''));
  const nodes = fullSnapshot.nodes;
  const strings = fullSnapshot.strings;
  const nodeFields = fullSnapshot.snapshot.meta.node_fields;
  const fieldCount = nodeFields.length;

  const promises = [];
  for (let i = 0; i < nodes.length; i += fieldCount) {
    const nameIdx = nodes[i + 1];
    const id = nodes[i + 2];
    const name = strings[nameIdx];
    if (name === 'null (wasm)') {
      promises.push(
          Protocol.HeapProfiler
              .getObjectByHeapObjectId({
                objectId: String(id),
              })
              .then(InspectorTest.logMessage));
    }
  }
  Promise.all(promises).then(InspectorTest.completeTest);
});
