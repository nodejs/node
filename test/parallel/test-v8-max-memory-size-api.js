'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');
const { Worker } = require('worker_threads');

if (!process.env.isWorker) {
  process.env.isWorker = true;
  new Worker(__filename, {
    resourceLimits: {
      maxYoungGenerationSizeMb: 50,
      maxOldGenerationSizeMb: 500,
    }
  });
  assert.ok(v8.getMaxYoungGenerationSize() > 0);
  assert.ok(v8.getMaxOldGenerationSize() > 0);
} else {
  assert.ok(v8.getMaxYoungGenerationSize() === 50 * 1024 * 1024);
  assert.ok(v8.getMaxOldGenerationSize() === 500 * 1024 * 1024);
}
