'use strict';
// This tests heap snapshot integration of inspector.

const common = require('../common');
const assert = require('assert');
common.skipIfInspectorDisabled();

const { validateByRetainingPath, validateByRetainingPathFromNodes } = require('../common/heap');
const inspector = require('inspector');

// Starts with no JSBindingsConnection.
{
  const nodes = validateByRetainingPath('Node / JSBindingsConnection', []);
  assert.strictEqual(nodes.length, 0);
}

// JSBindingsConnection should be added once inspector session is created.
{
  const session = new inspector.Session();
  session.connect();

  const nodes = validateByRetainingPath('Node / JSBindingsConnection', []);
  validateByRetainingPathFromNodes(nodes, 'Node / JSBindingsConnection', [
    { node_name: 'Node / InspectorSession', edge_name: 'session' },
  ]);
  validateByRetainingPathFromNodes(nodes, 'Node / JSBindingsConnection', [
    { node_name: 'Connection', edge_name: 'native_to_javascript' },
  ]);
}
