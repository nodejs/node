// Flags: --expose-internals
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const { validateSnapshotNodes } = require('../common/heap');
const inspector = require('inspector');

const session = new inspector.Session();
validateSnapshotNodes('INSPECTORJSBINDING', []);
session.connect();
validateSnapshotNodes('INSPECTORJSBINDING', [
  {
    children: [
      { name: 'session' },
      { name: 'Connection' },
      (node) => node.type === 'closure' || typeof node.value === 'function'
    ]
  }
]);
