var common = require('../common');
var assert = require('assert');

var MESSAGE = 'catch me if you can';
var caughtException = false;

process.addListener('uncaughtException', function(e) {
  console.log('uncaught exception! 1');
  assert.equal(MESSAGE, e.message);
  caughtException = true;
});

process.addListener('uncaughtException', function(e) {
  console.log('uncaught exception! 2');
  assert.equal(MESSAGE, e.message);
  caughtException = true;
});

setTimeout(function() {
  throw new Error(MESSAGE);
}, 10);

process.addListener('exit', function() {
  console.log('exit');
  assert.equal(true, caughtException);
});
