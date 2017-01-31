'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const file = path.join(common.fixturesDir, 'file', 'does', 'not', 'exist');
const errorMessage = `ENOENT: no such file or directory, open '${file}'`;

assert.throws(() => {
  fs.openSync(file, 'r');
}, (err) => {
  assert(err instanceof Error);
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.message, errorMessage);
  return true;
});

fs.open(__filename, 'r', common.mustCall((err) => {
  assert.ifError(err);
}));

fs.open(__filename, 'rs', common.mustCall((err) => {
  assert.ifError(err);
}));
