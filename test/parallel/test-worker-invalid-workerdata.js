'use strict';
require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// This tests verifies that failing to serialize workerData does not keep
// the process alive.
// Refs: https://github.com/nodejs/node/issues/22736

assert.throws(() => {
  new Worker('./worker.js', {
    workerData: { fn: () => {} }
  });
}, /DataCloneError/);
