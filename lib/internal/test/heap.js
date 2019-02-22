'use strict';

process.emitWarning(
  'These APIs are for internal testing only. Do not use them.',
  'internal/test/heap');

const { createHeapDump, buildEmbedderGraph } = internalBinding('heap_utils');
const assert = require('internal/assert');

// This is not suitable for production code. It creates a full V8 heap dump,
// parses it as JSON, and then creates complex objects from it, leading
// to significantly increased memory usage.
function createJSHeapDump() {
  const dump = createHeapDump();
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
      name: typeof name_or_index === 'string' ? name_or_index : null
    };
    toNode.incomingEdges.push(edge);
    fromNode.outgoingEdges.push(edge);
    edgeIndex++;
  }

  for (const node of nodes) {
    assert(node.edge_count === node.outgoingEdges.length,
           `${node.edge_count} !== ${node.outgoingEdges.length}`);
  }
  return nodes;
}

function readHeapInfo(raw, fields, types, strings) {
  const items = [];

  for (var i = 0; i < raw.length; i += fields.length) {
    const item = {};
    for (var j = 0; j < fields.length; j++) {
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

module.exports = {
  createJSHeapDump,
  buildEmbedderGraph
};
