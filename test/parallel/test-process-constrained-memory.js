'use strict';
require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');


if (!process.env.isWorker) {
  process.env.isWorker = true;
  new Worker(__filename);
  assert(process.constrainedMemory() >= 0);
} else {
  assert(process.constrainedMemory() >= 0);
}
