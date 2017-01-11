'use strict';
const common = require('../common');
const assert = require('assert');

process.stdout.on('error', common.mustCall(function(e) {
  assert(e instanceof Error, 'e is an Error');
  assert.strictEqual(e.message, 'process.stdout cannot be closed.');
}));
process.stdout.end();
