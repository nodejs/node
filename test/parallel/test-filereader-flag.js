'use strict';

const common = require('../common');
const assert = require('node:assert');
const { Worker } = require('node:worker_threads');

// Without --experimental-fileapi, the File API globals are not exposed.
assert.strictEqual(typeof FileReader, 'undefined');
assert.strictEqual(typeof FileReaderSync, 'undefined');
assert.strictEqual(typeof ProgressEvent, 'undefined');

const worker = new Worker(
  `const { parentPort } = require('node:worker_threads');
   parentPort.postMessage(typeof FileReaderSync);`,
  { eval: true },
);
worker.on('message', common.mustCall((data) => {
  assert.strictEqual(data, 'undefined');
}));
