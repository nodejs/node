'use strict';
const assert = require('assert');
const util = require('util');

let internalBinding;
try {
  internalBinding = require('internal/test/binding').internalBinding;
} catch (e) {
  console.log('using `test/common/heap.js` requires `--expose-internals`');
  throw e;
}

const { buildEmbedderGraph } = internalBinding('heap_utils');
const { getHeapSnapshot } = require('v8');

function createJSHeapSnapshot(stream = getHeapSnapshot()) {
  stream.pause();
  const dump = JSON.parse(stream.read());
  const meta = dump.snapshot.meta;

  const nodes =
    readHeapInfo(dump.nodes, meta.node_fields, meta.node_types, dump.strings);
  const edges =
    readHeapInfo(dump.edges, meta.edge_fields, meta.edge_types, dump.strings);

  for (const node of nodes) {
    node.incomingEdges = [];
    node.outgoingEdges = [];
  }

  let fromNodeIndex = 0;
  let edgeIndex = 0;
  for (const { type, name_or_index, to_node } of edges) {
    while (edgeIndex === nodes[fromNodeIndex].edge_count) {
      edgeIndex = 0;
      fromNodeIndex++;
    }
    const toNode = nodes[to_node / meta.node_fields.length];
    const fromNode = nodes[fromNodeIndex];
    const edge = {
      type,
      to: toNode,
      from: fromNode,
      name: typeof name_or_index === 'string' ? name_or_index : null,
    };
    toNode.incomingEdges.push(edge);
    fromNode.outgoingEdges.push(edge);
    edgeIndex++;
  }

  for (const node of nodes) {
    assert.strictEqual(node.edge_count, node.outgoingEdges.length,
                       `${node.edge_count} !== ${node.outgoingEdges.length}`);
  }
  return nodes;
}

function readHeapInfo(raw, fields, types, strings) {
  const items = [];

  for (let i = 0; i < raw.length; i += fields.length) {
    const item = {};
    for (let j = 0; j < fields.length; j++) {
      const name = fields[j];
      let type = types[j];
      if (Array.isArray(type)) {
        item[name] = type[raw[i + j]];
      } else if (name === 'name_or_index') {  // type === 'string_or_number'
        if (item.type === 'element' || item.type === 'hidden')
          type = 'number';
        else
          type = 'string';
      }

      if (type === 'string') {
        item[name] = strings[raw[i + j]];
      } else if (type === 'number' || type === 'node') {
        item[name] = raw[i + j];
      }
    }
    items.push(item);
  }

  return items;
}

function inspectNode(snapshot) {
  return util.inspect(snapshot, { depth: 4 });
}

function isEdge(edge, { node_name, edge_name }) {
  if (edge_name !== undefined && edge.name !== edge_name) {
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
  constructor(stream) {
    this.snapshot = createJSHeapSnapshot(stream);
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
            (node) => node.outgoingEdges.some(check),
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

      if (expectation.detachedness !== undefined) {
        const matchedNodes = rootNodes.filter(
          (node) => node.detachedness === expectation.detachedness);
        if (loose) {
          assert(matchedNodes.length >= rootNodes.length,
                 `Expect to find at least ${rootNodes.length} with ` +
                `detachedness ${expectation.detachedness}, ` +
                `found ${matchedNodes.length}`);
        } else {
          assert.strictEqual(
            matchedNodes.length, rootNodes.length,
            `Expect to find ${rootNodes.length} with detachedness ` +
            `${expectation.detachedness},  found ${matchedNodes.length}`);
        }
      }
    }
  }

  // Validate our internal embedded graph representation
  validateGraph(rootName, expected, { loose = false } = {}) {
    const rootNodes = this.embedderGraph.filter(
      (node) => node.name === rootName,
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
            (node) => node.edges.some(check),
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

function recordState(stream = undefined) {
  return new State(stream);
}

function validateSnapshotNodes(...args) {
  return recordState().validateSnapshotNodes(...args);
}

function getHeapSnapshotOptionTests() {
  const fixtures = require('../common/fixtures');
  const cases = [
    {
      options: { exposeInternals: true },
      expected: [{
        children: [
          // We don't have anything special to test here yet
          // because we don't use cppgc or embedder heap tracer.
          { edge_name: 'nonNumeric', node_name: 'test' },
        ],
      }],
    },
    {
      options: { exposeNumericValues: true },
      expected: [{
        children: [
          { edge_name: 'numeric', node_name: 'smi number' },
        ],
      }],
    },
  ];
  return {
    fixtures: fixtures.path('klass-with-fields.js'),
    check(snapshot, expected) {
      snapshot.validateSnapshot('Klass', expected, { loose: true });
    },
    cases,
  };
}

module.exports = {
  recordState,
  validateSnapshotNodes,
  getHeapSnapshotOptionTests,
};
