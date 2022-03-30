// Flags: --v8-pool-size=0
'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread } = require('worker_threads');

if (isMainThread) {
  const w = new Worker(__filename);
  w.on('online', common.mustCall());
  w.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
  process.on('exit', (code) => {
    assert.strictEqual(code, 0);
  });
}
