// Flags: --expose-internals
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const { validateSnapshotNodes } = require('../common/heap');
const inspector = require('inspector');

const snapshotNode = {
  children: [
    { node_name: 'Node / InspectorSession', edge_name: 'session' },
  ],
};

// Starts with no JSBindingsConnection (or 1 if coverage enabled).
{
  const expected = [];
  if (process.env.NODE_V8_COVERAGE) {
    expected.push(snapshotNode);
  }
  validateSnapshotNodes('Node / JSBindingsConnection', expected);
}

// JSBindingsConnection should be added.
{
  const session = new inspector.Session();
  session.connect();
  const expected = [
    {
      children: [
        { node_name: 'Node / InspectorSession', edge_name: 'session' },
        { node_name: 'Connection', edge_name: 'native_to_javascript' },
        (edge) => edge.name === 'callback' &&
          (edge.to.type === undefined || // embedded graph
           edge.to.type === 'closure'), // snapshot
      ],
    },
  ];
  if (process.env.NODE_V8_COVERAGE) {
    expected.push(snapshotNode);
  }
  validateSnapshotNodes('Node / JSBindingsConnection', expected);
}
