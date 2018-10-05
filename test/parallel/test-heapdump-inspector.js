// Flags: --expose-internals
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const { validateSnapshotNodes } = require('../common/heap');
const inspector = require('inspector');

const session = new inspector.Session();
validateSnapshotNodes('Node / JSBindingsConnection', []);
session.connect();
validateSnapshotNodes('Node / JSBindingsConnection', [
  {
    children: [
      { node_name: 'Node / InspectorSession', edge_name: 'session' },
      { node_name: 'Connection', edge_name: 'wrapped' },
      (edge) => (edge.to.type === undefined || // embedded graph
         edge.to.type === 'closure') // snapshot
    ]
  }
]);
