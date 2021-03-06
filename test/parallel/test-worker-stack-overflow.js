'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

const worker = new Worker('function f() { f(); } f();', { eval: true });

worker.on('error', common.expectsError({
  constructor: RangeError,
  message: 'Maximum call stack size exceeded'
}));
