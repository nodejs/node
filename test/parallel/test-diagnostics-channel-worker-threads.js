'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');
const dc = require('diagnostics_channel');

dc.subscribe('worker_threads', common.mustCall(({ worker }) => {
  assert.strictEqual(worker instanceof Worker, true);
}));

new Worker('const a = 1;', { eval: true });
