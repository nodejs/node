'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { Worker } = require('worker_threads');

const workerData = 'Hello from main thread';

const worker = new Worker(fixtures.path('worker-data.cjs'), {
  workerData
});

worker.on('message', common.mustCall((message) => {
  assert.strictEqual(message, workerData);
}));
