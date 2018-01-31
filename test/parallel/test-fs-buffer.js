'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
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

common.expectsError(
  () => {
    fs.accessSync(true);
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "path" argument must be one of type string, Buffer, or URL'
  }
);

const dir = Buffer.from(fixtures.fixturesDir);
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
