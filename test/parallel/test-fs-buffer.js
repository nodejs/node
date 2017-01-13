'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

common.refreshTmpDir();

assert.doesNotThrow(() => {
  fs.access(Buffer.from(common.tmpDir), common.mustCall((err) => {
    assert.ifError(err);
  }));
});

assert.doesNotThrow(() => {
  const buf = Buffer.from(path.join(common.tmpDir, 'a.txt'));
  fs.open(buf, 'w+', common.mustCall((err, fd) => {
    assert.ifError(err);
    assert(fd);
    fs.close(fd, common.mustCall(() => {
      fs.unlinkSync(buf);
    }));
  }));
});

assert.throws(() => {
  fs.accessSync(true);
}, /path must be a string or Buffer/);

const dir = Buffer.from(common.fixturesDir);
fs.readdir(dir, 'hex', common.mustCall((err, list) => {
  assert.ifError(err);
  list = list.map((i) => {
    return Buffer.from(i, 'hex').toString();
  });
  fs.readdir(dir, common.mustCall((err, list2) => {
    assert.ifError(err);
    assert.deepStrictEqual(list, list2);
  }));
}));
