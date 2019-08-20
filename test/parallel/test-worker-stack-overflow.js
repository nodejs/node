'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const worker = new Worker('function f() { f(); } f();', { eval: true });

worker.on('error', common.mustCall((err) => {
  assert.strictEqual(err.constructor, RangeError);
  assert.strictEqual(err.message, 'Maximum call stack size exceeded');
}));
