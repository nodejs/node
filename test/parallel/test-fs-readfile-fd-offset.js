'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const filename = path.join(common.tmpDir, 'readfile.txt');
const dataExpected = new Array(100).join('a');
fs.writeFileSync(filename, dataExpected);
const fileLength = dataExpected.length;

['r', 'a+'].forEach(mode => {
  const fd = fs.openSync(filename, mode);
  assert.strictEqual(fileLength, fs.readFileSync(fd).length);

  // Reading again should result in the same length.
  assert.strictEqual(fileLength, fs.readFileSync(fd).length);

  fs.readFile(fd, common.mustCall((err, buf) => {
    assert.ifError(err);
    assert.strictEqual(fileLength, buf.length);
  }));
});
