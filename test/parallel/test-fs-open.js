'use strict';
const common = require('../common');
var assert = require('assert');
var fs = require('fs');

var caughtException = false;
try {
  // should throw ENOENT, not EBADF
  // see https://github.com/joyent/node/pull/1228
  fs.openSync('/path/to/file/that/does/not/exist', 'r');
} catch (e) {
  assert.equal(e.code, 'ENOENT');
  caughtException = true;
}
assert.ok(caughtException);

fs.open(__filename, 'r', common.mustCall(function(err, fd) {
  if (err) {
    throw err;
  }
}));

fs.open(__filename, 'rs', common.mustCall(function(err, fd) {
  if (err) {
    throw err;
  }
}));
