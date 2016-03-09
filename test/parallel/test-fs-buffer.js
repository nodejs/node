'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

common.refreshTmpDir();

assert.doesNotThrow(() => {
  fs.access(Buffer.from(common.tmpDir), common.mustCall((err) => {
    if (err) throw err;
  }));
});

assert.doesNotThrow(() => {
  const buf = Buffer.from(path.join(common.tmpDir, 'a.txt'));
  fs.open(buf, 'w+', common.mustCall((err, fd) => {
    if (err) throw err;
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
  if (err) throw err;
  list = list.map((i) => {
    return Buffer.from(i, 'hex').toString();
  });
  fs.readdir(dir, common.mustCall((err, list2) => {
    if (err) throw err;
    assert.deepStrictEqual(list, list2);
  }));
}));
