// Flags: --expose-internals --experimental-worker
'use strict';
require('../common');
const { validateSnapshotNodes } = require('../common/heap');
const { Worker } = require('worker_threads');

validateSnapshotNodes('Worker', []);
const worker = new Worker('setInterval(() => {}, 100);', { eval: true });
validateSnapshotNodes('Worker', [
  {
    children: [
      { name: 'thread_exit_async' },
      { name: 'env' },
      { name: 'MessagePort' },
      { name: 'Worker' }
    ]
  }
]);
validateSnapshotNodes('MessagePort', [
  {
    children: [
      { name: 'MessagePortData' },
      { name: 'MessagePort' }
    ]
  }
], { loose: true });
worker.terminate();
