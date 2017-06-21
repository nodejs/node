'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

common.refreshTmpDir();

const fn = path.join(common.tmpDir, 'write-string-coerce.txt');
const data = true;
const expected = String(data);

fs.open(fn, 'w', 0o644, common.mustCall(function(err, fd) {
  assert.ifError(err);
  console.log('open done');
  fs.write(fd, data, 0, 'utf8', common.mustCall(function(err, written) {
    console.log('write done');
    assert.ifError(err);
    assert.strictEqual(Buffer.byteLength(expected), written);
    fs.closeSync(fd);
    const found = fs.readFileSync(fn, 'utf8');
    console.log('expected: "%s"', expected);
    console.log('found: "%s"', found);
    fs.unlinkSync(fn);
    assert.strictEqual(expected, found);
  }));
}));
