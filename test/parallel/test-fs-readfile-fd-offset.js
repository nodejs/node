'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const filename = path.join(common.tmpDir, 'readfile.txt');
const dataExpected = 'a'.repeat(100);
fs.writeFileSync(filename, dataExpected);
const fileLength = dataExpected.length;

['r', 'a+'].forEach((mode) => {
  const fd = fs.openSync(filename, mode);
  assert.strictEqual(fs.readFileSync(fd).length, fileLength);

  // Reading again should result in the same length.
  assert.strictEqual(fs.readFileSync(fd).length, fileLength);

  fs.readFile(fd, common.mustCall((err, buf) => {
    assert.ifError(err);
    assert.strictEqual(buf.length, fileLength);
  }));
});
