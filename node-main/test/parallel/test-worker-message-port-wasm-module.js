'use strict';
const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const { Worker } = require('worker_threads');
const wasmModule = new WebAssembly.Module(fixtures.readSync('simple.wasm'));

const worker = new Worker(`
const { parentPort } = require('worker_threads');
parentPort.once('message', ({ wasmModule }) => {
  const instance = new WebAssembly.Instance(wasmModule);
  parentPort.postMessage(instance.exports.add(10, 20));
});
`, { eval: true });

worker.once('message', common.mustCall((num) => assert.strictEqual(num, 30)));
worker.postMessage({ wasmModule });
