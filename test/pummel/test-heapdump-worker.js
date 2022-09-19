// Flags: --expose-internals
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const { Worker } = require('worker_threads');

validateSnapshotNodes('Node / Worker', []);
const worker = new Worker('setInterval(() => {}, 100);', { eval: true });
validateSnapshotNodes('Node / MessagePort', [
  {
    children: [
      { node_name: 'Node / MessagePortData', edge_name: 'data' },
    ]
  },
], { loose: true });
worker.terminate();
