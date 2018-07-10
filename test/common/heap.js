/* eslint-disable node-core/required-modules */
'use strict';
const assert = require('assert');
const util = require('util');

let internalTestHeap;
try {
  internalTestHeap = require('internal/test/heap');
} catch (e) {
  console.log('using `test/common/heap.js` requires `--expose-internals`');
  throw e;
}
const { createJSHeapDump, buildEmbedderGraph } = internalTestHeap;

class State {
  constructor() {
    this.snapshot = createJSHeapDump();
    this.embedderGraph = buildEmbedderGraph();
  }

  validateSnapshotNodes(name, expected, { loose = false } = {}) {
    const snapshot = this.snapshot.filter(
      (node) => node.name === 'Node / ' + name && node.type !== 'string');
    if (loose)
      assert(snapshot.length >= expected.length);
    else
      assert.strictEqual(snapshot.length, expected.length);
    for (const expectedNode of expected) {
      if (expectedNode.children) {
        for (const expectedChild of expectedNode.children) {
          const check = typeof expectedChild === 'function' ?
            expectedChild :
            (node) => [expectedChild.name, 'Node / ' + expectedChild.name]
              .includes(node.name);

          assert(snapshot.some((node) => {
            return node.outgoingEdges.map((edge) => edge.toNode).some(check);
          }), `expected to find child ${util.inspect(expectedChild)} ` +
              `in ${util.inspect(snapshot)}`);
        }
      }
    }

    const graph = this.embedderGraph.filter((node) => node.name === name);
    if (loose)
      assert(graph.length >= expected.length);
    else
      assert.strictEqual(graph.length, expected.length);
    for (const expectedNode of expected) {
      if (expectedNode.edges) {
        for (const expectedChild of expectedNode.children) {
          const check = typeof expectedChild === 'function' ?
            expectedChild : (node) => {
              return node.name === expectedChild.name ||
                (node.value &&
                 node.value.constructor &&
                 node.value.constructor.name === expectedChild.name);
            };

          assert(graph.some((node) => node.edges.some(check)),
                 `expected to find child ${util.inspect(expectedChild)} ` +
                 `in ${util.inspect(snapshot)}`);
        }
      }
    }
  }
}

function recordState() {
  return new State();
}

function validateSnapshotNodes(...args) {
  return recordState().validateSnapshotNodes(...args);
}

module.exports = {
  recordState,
  validateSnapshotNodes
};
