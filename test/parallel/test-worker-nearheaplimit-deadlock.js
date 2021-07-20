'use strict';
const common = require('../common');
const assert = require('assert');
const { isMainThread, Worker } = require('worker_threads');

if (isMainThread) {
  const opts = {
    resourceLimits: {
      maxYoungGenerationSizeMb: 0,
      maxOldGenerationSizeMb: 0
    }
  };

  const worker = new Worker(__filename, opts);
  worker.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_WORKER_OUT_OF_MEMORY');
  }));
} else {
  setInterval(() => {}, 1);
}
