'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');
const { once } = require('events');

// Ensure that worker.getHeapSnapshot() returns a valid JSON
(async () => {
  const worker = new Worker('setInterval(() => {}, 1000);', { eval: true });
  await once(worker, 'online');
  const stream = await worker.getHeapSnapshot();
  stream.read(0); // Trigger the stream to start flowing
  assert.ok(JSON.parse(stream.read(stream.readableLength)));

  await worker.terminate();
})().then(common.mustCall());
