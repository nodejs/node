'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

process.on('uncaughtException', common.mustCall());

new Worker('', { eval: true })
  .on('exit', common.mustCall(() => { throw new Error('foo'); }));
