'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

let caughtException = false;

try {
  // should throw ENOENT, not EBADF
  // see https://github.com/joyent/node/pull/1228
  fs.openSync('/path/to/file/that/does/not/exist', 'r');
} catch (e) {
  assert.strictEqual(e.code, 'ENOENT');
  caughtException = true;
}
assert.strictEqual(caughtException, true);

fs.open(__filename, 'r', common.mustCall((err) => {
  assert.ifError(err);
}));

fs.open(__filename, 'rs', common.mustCall((err) => {
  assert.ifError(err);
}));
