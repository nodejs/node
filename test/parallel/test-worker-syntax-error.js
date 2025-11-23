'use strict';
const common = require('../common');
if (!process.config.variables.node_use_amaro) {
  common.skip('Requires Amaro');
}
const assert = require('assert');
const { Worker } = require('worker_threads');

const w = new Worker('abc)', { eval: true });
w.on('message', common.mustNotCall());
w.on('error', common.mustCall((err) => {
  assert.strictEqual(err.constructor, SyntaxError);
  assert.strictEqual(err.name, 'SyntaxError');
}));
