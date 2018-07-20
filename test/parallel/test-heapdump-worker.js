// Flags: --expose-internals --experimental-worker
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const { Worker } = require('worker_threads');

validateSnapshotNodes('WORKER', []);
const worker = new Worker('setInterval(() => {}, 100);', { eval: true });
validateSnapshotNodes('WORKER', [
  {
    children: [
      { name: 'thread_exit_async' },
      { name: 'env' },
      { name: 'MESSAGEPORT' },
      { name: 'Worker' }
    ]
  }
]);
validateSnapshotNodes('MESSAGEPORT', [
  {
    children: [
      { name: 'data' },
      { name: 'MessagePort' }
    ]
  }
], { loose: true });
worker.terminate();
