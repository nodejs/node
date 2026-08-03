// Flags: --no-node-snapshot
// With node snapshot the OOM can occur during the deserialization of the
// context, so disable it since we want the OOM to occur during the creation of
// the message port.
'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const opts = {
    resourceLimits: {
      maxYoungGenerationSizeMb: 0,
      maxOldGenerationSizeMb: 0
    },
  };

  const worker = new Worker(__filename, opts);
  worker.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_WORKER_OUT_OF_MEMORY');
  }));
} else {
  setInterval(() => {}, 1);
}
