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
    fs.close(fd, common.mustCall((err) => {
      assert.ifError(err);
    }));
  }));
});

assert.throws(() => {
  fs.accessSync(true);
}, /path must be a string or Buffer/);

const dir = Buffer.from(common.fixturesDir);
fs.readdir(dir, 'hex', common.mustCall((err, hexList) => {
  assert.ifError(err);
  fs.readdir(dir, common.mustCall((err, stringList) => {
    assert.ifError(err);
    stringList.forEach((val, idx) => {
      const fromHexList = Buffer.from(hexList[idx], 'hex').toString();
      assert.strictEqual(
        fromHexList,
        val,
        `expected ${val}, got ${fromHexList} by hex decoding ${hexList[idx]}`
      );
    });
  }));
}));
