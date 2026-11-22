// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-incremental-marking

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that custom formatter error strings are not retained in group 0 ' +
    'when context is destroyed.');

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

function NodeByName(snapshot, name) {
  let count = snapshot['nodes'].length / kNodeSize;
  for (let i = 0; i < count; i++) {
    if (NodeName(snapshot, i * kNodeSize) == name) return i * kNodeSize;
  }
  return -1;
}

InspectorTest.runAsyncTestSuite([async function testMemoryLeak() {
  await Protocol.Runtime.enable();
  await Protocol.HeapProfiler.enable();
  await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});

  Protocol.Runtime.onConsoleAPICalled(
      message => {
          // Suppress console API log output to keep expected.txt clean.
      });

  contextGroup.createContext('destroyed-context');
  const {params: {context: {uniqueId}}} =
      await Protocol.Runtime.onceExecutionContextCreated();

  await Protocol.Runtime.evaluate({
    expression: `
        (() => {
          class MyLeakedObject {};
          globalThis.devtoolsFormatters = [{
            header(o) {
              if (o instanceof MyLeakedObject) {
                inspector.fireContextDestroyed();
                throw new Error('MyUniqueLeakedString');
              }
              return null;
            },
            hasBody() { return false; }
          }];
          console.log(new MyLeakedObject());
        })();
      `,
    uniqueContextId: uniqueId,
  });

  // Now the context is destroyed.
  // The error string should not be retained in group 0 storage.
  let snapshot_string = '';
  function onChunk(message) {
    snapshot_string += message['params']['chunk'];
  }
  Protocol.HeapProfiler.onAddHeapSnapshotChunk(onChunk);
  await Protocol.HeapProfiler.collectGarbage();
  await Protocol.HeapProfiler.takeHeapSnapshot({reportProgress: false});
  let snapshot = JSON.parse(snapshot_string);

  let found_strings = [];
  for (let i = 0; i < snapshot['strings'].length; i++) {
    let s = snapshot['strings'][i];
    if (s.includes('Uncaught Error: MyUniqueLeakedString')) {
      found_strings.push(s);
    }
  }

  let leak_detected = false;
  if (found_strings.length > 0) {
    let targetIndex = NodeByName(snapshot, found_strings[0]);
    let consStringIndex = -1;
    if (targetIndex !== -1) {
      let count = snapshot['nodes'].length / kNodeSize;
      let edgeIndex = 0;
      for (let i = 0; i < count; i++) {
        let edgeCount = EdgeCount(snapshot, i * kNodeSize);
        for (let j = 0; j < edgeCount; j++) {
          if (EdgeTarget(snapshot, edgeIndex) === targetIndex) {
            if (NodeName(snapshot, i * kNodeSize) === '(concatenated string)') {
              consStringIndex = i * kNodeSize;
            }
          }
          edgeIndex += kEdgeSize;
        }
      }
    }
    if (consStringIndex !== -1) {
      let count = snapshot['nodes'].length / kNodeSize;
      let edgeIndex = 0;
      for (let i = 0; i < count; i++) {
        let edgeCount = EdgeCount(snapshot, i * kNodeSize);
        for (let j = 0; j < edgeCount; j++) {
          if (EdgeTarget(snapshot, edgeIndex) === consStringIndex) {
            if (NodeName(snapshot, i * kNodeSize) === '(Global handles)') {
              let edgeName = EdgeName(snapshot, edgeIndex);
              if (edgeName.includes('DevTools console')) {
                leak_detected = true;
              }
            }
          }
          edgeIndex += kEdgeSize;
        }
      }
    }
  }

  if (leak_detected) {
    InspectorTest.log('Leak detected.');
  } else {
    InspectorTest.log('No leak detected.');
  }

  await Protocol.Runtime.disable();
  await Protocol.HeapProfiler.disable();
}]);
