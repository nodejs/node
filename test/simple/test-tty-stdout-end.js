// Can't test this when 'make test' doesn't assign a tty to the stdout.
var common = require('../common');
var assert = require('assert');
var tty = require('tty');

var closed = false;
process.stdout.on('close', function() {
  closed = true;
});
process.on('exit', function() {
  assert.ok(closed);
});

process.stdout.end();
