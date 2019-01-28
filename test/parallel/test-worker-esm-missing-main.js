'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const worker = new Worker('./does-not-exist.js', {
  execArgv: ['--experimental-modules'],
});

worker.on('error', common.mustCall((err) => {
  // eslint-disable-next-line node-core/no-unescaped-regexp-dot
  assert(/Cannot find module .+does-not-exist.js/.test(err.message), err);
}));
