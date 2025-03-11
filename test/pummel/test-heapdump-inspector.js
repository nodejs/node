'use strict';
// This tests heap snapshot integration of inspector.

const common = require('../common');
const assert = require('assert');
common.skipIfInspectorDisabled();

const { findByRetainingPath, findByRetainingPathFromNodes } = require('../common/heap');
const inspector = require('inspector');

// Starts with no JSBindingsConnection.
{
  const nodes = findByRetainingPath('Node / JSBindingsConnection', []);
  assert.strictEqual(nodes.length, 0);
}

// JSBindingsConnection should be added once inspector session is created.
{
  const session = new inspector.Session();
  session.connect();

  const nodes = findByRetainingPath('Node / JSBindingsConnection', []);
  findByRetainingPathFromNodes(nodes, 'Node / JSBindingsConnection', [
    { node_name: 'Node / InspectorSession', edge_name: 'session' },
  ]);
  findByRetainingPathFromNodes(nodes, 'Node / JSBindingsConnection', [
    { node_name: 'Connection', edge_name: 'native_to_javascript' },
  ]);
}
