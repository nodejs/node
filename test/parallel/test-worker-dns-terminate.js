// Flags: --experimental-worker
'use strict';
const common = require('../common');
const { Worker } = require('worker');

const w = new Worker(`
const dns = require('dns');
dns.lookup('nonexistent.org', () => {});
require('worker').parentPort.postMessage('0');
`, { eval: true });

w.on('message', common.mustCall(() => {
  // This should not crash the worker during a DNS request.
  w.terminate(common.mustCall());
}));
