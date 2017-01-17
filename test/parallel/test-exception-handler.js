'use strict';
const common = require('../common');
const assert = require('assert');

const MESSAGE = 'catch me if you can';

process.on('uncaughtException', common.mustCall(function(e) {
  console.log('uncaught exception! 1');
  assert.strictEqual(MESSAGE, e.message);
}));

process.on('uncaughtException', common.mustCall(function(e) {
  console.log('uncaught exception! 2');
  assert.strictEqual(MESSAGE, e.message);
}));

setTimeout(function() {
  throw new Error(MESSAGE);
}, 10);
