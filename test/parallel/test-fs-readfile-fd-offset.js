'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const fileLength = fs.statSync(__filename).size;

['r', 'a+'].forEach(mode => {
  const fd = fs.openSync(__filename, mode);
  assert.strictEqual(fileLength, fs.readFileSync(fd).length);

  // Reading again should result in the same length.
  assert.strictEqual(fileLength, fs.readFileSync(fd).length);

  fs.readFile(fd, common.mustCall((err, buf) => {
    assert.ifError(err);
    assert.strictEqual(fileLength, buf.length);
  }));
});
