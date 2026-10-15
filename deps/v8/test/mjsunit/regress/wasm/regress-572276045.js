// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const table = new WebAssembly.Table({ element: 'structref', initial: 1 });

const snapshotChunks = [];

function receive(msgStr) {
  const msg = JSON.parse(msgStr);

  if (msg.method === 'HeapProfiler.addHeapSnapshotChunk') {
    snapshotChunks.push(msg.params.chunk);
  } else if (msg.id === 1) {
    const fullSnapshot = JSON.parse(snapshotChunks.join(''));
    const nodes = fullSnapshot.nodes;
    const strings = fullSnapshot.strings;
    const nodeFields = fullSnapshot.snapshot.meta.node_fields;
    const fieldCount = nodeFields.length;

    for (let i = 0; i < nodes.length; i += fieldCount) {
      const nameIdx = nodes[i + 1];
      const id = nodes[i + 2];
      const name = strings[nameIdx];
      if (name === 'null (wasm)') {
        send(JSON.stringify({
          id: 2,
          method: 'HeapProfiler.getObjectByHeapObjectId',
          params: { objectId: String(id) }
        }));
      }
    }
  }
}
send(JSON.stringify({ id: 1, method: 'HeapProfiler.takeHeapSnapshot' }));
