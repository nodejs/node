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

          const hasChild = snapshot.some((node) => {
            return node.outgoingEdges.map((edge) => edge.toNode).some(check);
          });
          // Don't use assert with a custom message here. Otherwise the
          // inspection in the message is done eagerly and wastes a lot of CPU
          // time.
          if (!hasChild) {
            throw new Error(
              'expected to find child ' +
              `${util.inspect(expectedChild)} in ${util.inspect(snapshot)}`);
          }
        }
      }
    }

    const graph = this.embedderGraph.filter((node) => node.name === name);
    if (loose)
      assert(graph.length >= expected.length);
    else
      assert.strictEqual(graph.length, expected.length);
    for (const expectedNode of expected) {
      if (expectedNode.children) {
        for (const expectedChild of expectedNode.children) {
          const check = (edge) => {
            // TODO(joyeecheung): check the edge names
            const node = edge.to;
            if (typeof expectedChild === 'function') {
              return expectedChild(node);
            }
            return node.name === expectedChild.name ||
              (node.value &&
                node.value.constructor &&
                node.value.constructor.name === expectedChild.name);
          };

          // Don't use assert with a custom message here. Otherwise the
          // inspection in the message is done eagerly and wastes a lot of CPU
          // time.
          const hasChild = graph.some((node) => node.edges.some(check));
          if (!hasChild) {
            throw new Error(
              'expected to find child ' +
              `${util.inspect(expectedChild)} in ${util.inspect(snapshot)}`);
          }
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
