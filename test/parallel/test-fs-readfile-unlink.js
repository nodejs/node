'use strict';
const assert = require('assert');
const common = require('../common');
const fs = require('fs');
const path = require('path');
const dirName = path.resolve(common.fixturesDir, 'test-readfile-unlink');
const fileName = path.resolve(dirName, 'test.bin');

var buf = new Buffer(512 * 1024);
buf.fill(42);

try {
  fs.mkdirSync(dirName);
} catch (e) {
  // Ignore if the directory already exists.
  if (e.code != 'EEXIST') throw e;
}

fs.writeFileSync(fileName, buf);

fs.readFile(fileName, function(err, data) {
  assert.ifError(err);
  assert(data.length == buf.length);
  assert.strictEqual(buf[0], 42);

  fs.unlinkSync(fileName);
  fs.rmdirSync(dirName);
});
