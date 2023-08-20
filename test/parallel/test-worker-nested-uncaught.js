'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

// Regression test for https://github.com/nodejs/node/issues/34309

const w = new Worker(
  `const { Worker } = require('worker_threads');
  new Worker("throw new Error('uncaught')", { eval:true })`,
  { eval: true });
w.on('error', common.expectsError({
  name: 'Error',
  message: 'uncaught'
}));
