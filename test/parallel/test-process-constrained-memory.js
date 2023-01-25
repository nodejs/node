'use strict';
require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');
const constrainedMemory = process.constrainedMemory();
if (constrainedMemory !== undefined) {
  assert(constrainedMemory > 0);
}
if (!process.env.isWorker) {
  process.env.isWorker = true;
  new Worker(__filename);
}
