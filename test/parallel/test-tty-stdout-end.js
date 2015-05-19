'use strict';
// Can't test this when 'make test' doesn't assign a tty to the stdout.
var common = require('../common');
var assert = require('assert');

var exceptionCaught = false;

try {
  process.stdout.end();
} catch (e) {
  exceptionCaught = true;
  assert.ok(common.isError(e));
  assert.equal('process.stdout cannot be closed.', e.message);
}

assert.ok(exceptionCaught);
