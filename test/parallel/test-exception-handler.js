'use strict';
const common = require('../common');
var assert = require('assert');

var MESSAGE = 'catch me if you can';

process.on('uncaughtException', common.mustCall(function(e) {
  console.log('uncaught exception! 1');
  assert.equal(MESSAGE, e.message);
}));

process.on('uncaughtException', common.mustCall(function(e) {
  console.log('uncaught exception! 2');
  assert.equal(MESSAGE, e.message);
}));

setTimeout(function() {
  throw new Error(MESSAGE);
}, 10);
