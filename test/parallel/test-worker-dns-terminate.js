'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');

const w = new Worker(`
const dns = require('dns');
dns.lookup('nonexistent.org', () => {});
require('worker_threads').parentPort.postMessage('0');
`, { eval: true });

w.on('message', common.mustCall(() => {
  // This should not crash the worker during a DNS request.
  w.terminate().then(common.mustCall());
}));
