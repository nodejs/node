// Flags: --expose-internals
'use strict';

// This tests that Environment is tracked in heap snapshots.

require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const assert = require('assert');

// This is just using ContextifyScript as an example here, it can be replaced
// with any BaseObject that we can easily instantiate here and register in
// cleanup hooks.
// These can all be changed to reflect the status of how these objects
// are captured in the snapshot.
const context = require('vm').createScript('const foo = 123');

validateSnapshotNodes('Node / Environment', [{
  children: [
    cleanupHooksFilter,
    { node_name: 'Node / cleanup_hooks', edge_name: 'cleanup_hooks' },
    { node_name: 'process', edge_name: 'process_object' },
    { node_name: 'Node / IsolateData', edge_name: 'isolate_data' },
  ]
}]);

function cleanupHooksFilter(edge) {
  if (edge.name !== 'cleanup_hooks') {
    return false;
  }
  if (edge.to.type === 'native') {
    verifyCleanupHooksInSnapshot(edge.to);
  } else {
    verifyCleanupHooksInGraph(edge.to);
  }
  return true;
}

function verifyCleanupHooksInSnapshot(node) {
  assert.strictEqual(node.name, 'Node / cleanup_hooks');
  const baseObjects = [];
  for (const hook of node.outgoingEdges) {
    for (const hookEdge of hook.to.outgoingEdges) {
      if (hookEdge.name === 'arg') {
        baseObjects.push(hookEdge.to);
      }
    }
  }
  // Make sure our ContextifyScript show up.
  assert(baseObjects.some((node) => node.name === 'Node / ContextifyScript'));
}

function verifyCleanupHooksInGraph(node) {
  assert.strictEqual(node.name, 'Node / cleanup_hooks');
  const baseObjects = [];
  for (const hook of node.edges) {
    for (const hookEdge of hook.to.edges) {
      if (hookEdge.name === 'arg') {
        baseObjects.push(hookEdge.to);
      }
    }
  }
  // Make sure our ContextifyScript show up.
  assert(baseObjects.some((node) => node.name === 'Node / ContextifyScript'));
}

console.log(context);  // Make sure it's not GC'ed
