// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-incremental-marking

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that objects logged during context destruction are not retained forever.');

const kNodeName = 1;
const kNodeEdgeCount = 4;
const kNodeSize = 6;
const kEdgeName = 1;
const kEdgeTarget = 2;
const kEdgeSize = 3;

function EdgeName(snapshot, edgeIndex) {
  return snapshot['strings'][snapshot['edges'][edgeIndex + kEdgeName]];
}

function EdgeTarget(snapshot, edgeIndex) {
  return snapshot['edges'][edgeIndex + kEdgeTarget];
}

function EdgeCount(snapshot, nodeIndex) {
  return snapshot['nodes'][nodeIndex + kNodeEdgeCount];
}

function NodeName(snapshot, nodeIndex) {
  return snapshot['strings'][snapshot['nodes'][nodeIndex + kNodeName]];
}

function NodeEdges(snapshot, nodeIndex) {
  let startEdgeIndex = 0;
  for (let i = 0; i < nodeIndex; i += kNodeSize) {
    startEdgeIndex += EdgeCount(snapshot, i);
  }
  let endEdgeIndex = startEdgeIndex + EdgeCount(snapshot, nodeIndex);
  let result = [];
  for (let i = startEdgeIndex; i < endEdgeIndex; ++i) {
    result.push(i * kEdgeSize);
  }
  return result;
}

function NodeByName(snapshot, name) {
  let count = snapshot['nodes'].length / kNodeSize;
  for (let i = 0; i < count; i++) {
    if (NodeName(snapshot, i * kNodeSize) == name) return i * kNodeSize;
  }
  return -1;
}

function GlobalHandleEdgeNames(snapshot, targetName) {
  let targetIndex = NodeByName(snapshot, targetName);
  if (targetIndex === -1) return [];
  let sourceIndex = NodeByName(snapshot, '(Global handles)');
  if (sourceIndex === -1) return [];

  let edges = NodeEdges(snapshot, sourceIndex);
  let results = [];
  for (let edge of edges) {
    if (EdgeTarget(snapshot, edge) == targetIndex) {
      let edgeName = EdgeName(snapshot, edge);
      results.push(edgeName.substring(edgeName.indexOf('/') + 2));
    }
  }
  return results;
}

InspectorTest.runAsyncTestSuite([async function testMemoryLeak() {
  await Protocol.Runtime.enable();
  await Protocol.HeapProfiler.enable();

  contextGroup.createContext('destroyed-context');
  const {params: {context: {uniqueId}}} =
      await Protocol.Runtime.onceExecutionContextCreated();

  await Protocol.Runtime.evaluate({
    expression: `
        (() => {
          class MyLeakedObject extends Error {};
          const err = new MyLeakedObject();
          Object.defineProperty(err, 'name', {
            get() {
              inspector.fireContextDestroyed();
              return 'Error';
            },
          });
          console.log(err);
        })();
      `,
    uniqueContextId: uniqueId,
  });

  // Now the context is destroyed.
  // The console.log should not retain MyLeakedObject in group 0 storage.
  let snapshot_string = '';
  function onChunk(message) {
    snapshot_string += message['params']['chunk'];
  }
  Protocol.HeapProfiler.onAddHeapSnapshotChunk(onChunk);
  await Protocol.HeapProfiler.collectGarbage();
  await Protocol.HeapProfiler.takeHeapSnapshot({reportProgress: false});
  let snapshot = JSON.parse(snapshot_string);
  let edges = GlobalHandleEdgeNames(snapshot, 'MyLeakedObject');
  if (edges.length > 0) {
    InspectorTest.log(
        `Leak detected: edge from (Global handles) to MyLeakedObject: ${
            edges.join(', ')}`);
  } else {
    InspectorTest.log('No leak detected.');
  }

  await Protocol.Runtime.disable();
  await Protocol.HeapProfiler.disable();
}]);
