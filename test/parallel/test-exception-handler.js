'use strict';
var common = require('../common');
var assert = require('assert');

var MESSAGE = 'catch me if you can';
var caughtException = false;

process.on('uncaughtException', function(e) {
  console.log('uncaught exception! 1');
  assert.equal(MESSAGE, e.message);
  caughtException = true;
});

process.on('uncaughtException', function(e) {
  console.log('uncaught exception! 2');
  assert.equal(MESSAGE, e.message);
  caughtException = true;
});

setTimeout(function() {
  throw new Error(MESSAGE);
}, 10);

process.on('exit', function() {
  console.log('exit');
  assert.equal(true, caughtException);
});
