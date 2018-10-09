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

function inspectNode(snapshot) {
  return util.inspect(snapshot, { depth: 4 });
}

function isEdge(edge, { node_name, edge_name }) {
  if (edge.name !== edge_name) {
    return false;
  }
  // From our internal embedded graph
  if (edge.to.value) {
    if (edge.to.value.constructor.name !== node_name) {
      return false;
    }
  } else if (edge.to.name !== node_name) {
    return false;
  }
  return true;
}

class State {
  constructor() {
    this.snapshot = createJSHeapDump();
    this.embedderGraph = buildEmbedderGraph();
  }

  // Validate the v8 heap snapshot
  validateSnapshot(rootName, expected, { loose = false } = {}) {
    const rootNodes = this.snapshot.filter(
      (node) => node.name === rootName && node.type !== 'string');
    if (loose) {
      assert(rootNodes.length >= expected.length,
             `Expect to find at least ${expected.length} '${rootName}', ` +
             `found ${rootNodes.length}`);
    } else {
      assert.strictEqual(
        rootNodes.length, expected.length,
        `Expect to find ${expected.length} '${rootName}', ` +
        `found ${rootNodes.length}`);
    }

    for (const expectation of expected) {
      if (expectation.children) {
        for (const expectedEdge of expectation.children) {
          const check = typeof expectedEdge === 'function' ? expectedEdge :
            (edge) => (isEdge(edge, expectedEdge));
          const hasChild = rootNodes.some(
            (node) => node.outgoingEdges.some(check)
          );
          // Don't use assert with a custom message here. Otherwise the
          // inspection in the message is done eagerly and wastes a lot of CPU
          // time.
          if (!hasChild) {
            throw new Error(
              'expected to find child ' +
              `${util.inspect(expectedEdge)} in ${inspectNode(rootNodes)}`);
          }
        }
      }
    }
  }

  // Validate our internal embedded graph representation
  validateGraph(rootName, expected, { loose = false } = {}) {
    const rootNodes = this.embedderGraph.filter(
      (node) => node.name === rootName
    );
    if (loose) {
      assert(rootNodes.length >= expected.length,
             `Expect to find at least ${expected.length} '${rootName}', ` +
             `found ${rootNodes.length}`);
    } else {
      assert.strictEqual(
        rootNodes.length, expected.length,
        `Expect to find ${expected.length} '${rootName}', ` +
        `found ${rootNodes.length}`);
    }
    for (const expectation of expected) {
      if (expectation.children) {
        for (const expectedEdge of expectation.children) {
          const check = typeof expectedEdge === 'function' ? expectedEdge :
            (edge) => (isEdge(edge, expectedEdge));
          // Don't use assert with a custom message here. Otherwise the
          // inspection in the message is done eagerly and wastes a lot of CPU
          // time.
          const hasChild = rootNodes.some(
            (node) => node.edges.some(check)
          );
          if (!hasChild) {
            throw new Error(
              'expected to find child ' +
              `${util.inspect(expectedEdge)} in ${inspectNode(rootNodes)}`);
          }
        }
      }
    }
  }

  validateSnapshotNodes(rootName, expected, { loose = false } = {}) {
    this.validateSnapshot(rootName, expected, { loose });
    this.validateGraph(rootName, expected, { loose });
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
