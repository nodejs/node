// Flags: --expose-gc
'use strict';
require('../common');
const { Worker } = require('worker_threads');
const { testGCProfiler, emitGC } = require('../common/v8');

if (process.env.isWorker) {
  process.env.isWorker = 1;
  new Worker(__filename);
} else {
  testGCProfiler();
  emitGC();
}
