'use strict';
var common = require('../common');
var assert = require('assert');

process.on('uncaughtException', function(err) {
  console.log('Caught exception: ' + err);
});

var timeoutFired = false;
setTimeout(function() {
  console.log('This will still run.');
  timeoutFired = true;
}, 500);

process.on('exit', function() {
  assert.ok(timeoutFired);
});

// Intentionally cause an exception, but don't catch it.
nonexistentFunc();
console.log('This will not run.');

