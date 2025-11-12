// Flags: --expose-gc
'use strict';
require('../common');
const { Worker } = require('worker_threads');
const { testGCProfiler } = require('../common/v8');

if (!process.env.isWorker) {
  process.env.isWorker = 1;
  new Worker(__filename);
} else {
  testGCProfiler();
  for (let i = 0; i < 100; i++) {
    new Array(100);
  }
  global?.gc();
}
