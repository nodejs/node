'use strict';
const common = require('../../common');
const assert = require('assert');
const path = require('path');
const { Worker } = require('worker_threads');
const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

const w = new Worker(`
require('worker_threads').parentPort.postMessage(
  require(${JSON.stringify(binding)}).hello());`, { eval: true });
w.on('message', common.mustCall((message) => {
  assert.strictEqual(message, 'world');
}));
