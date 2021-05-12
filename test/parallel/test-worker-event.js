'use strict';

const common = require('../common');
const assert = require('assert');
const {
  Worker,
} = require('worker_threads');

process.on('worker', common.mustCall(({ threadId }) => {
  assert.strictEqual(threadId, 1);
}));

new Worker('', { eval: true });
