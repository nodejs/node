'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const worker = new Worker('./does-not-exist.js', {
  execArgv: ['--experimental-modules'],
  stderr: true
});

let stderr = '';
worker.stderr.setEncoding('utf8');
worker.stderr.on('data', (chunk) => stderr += chunk);
worker.stderr.on('end', common.mustCall(() => {
  // eslint-disable-next-line node-core/no-unescaped-regexp-dot
  assert(/Cannot find module .+does-not-exist.js/.test(stderr), stderr);
}));
