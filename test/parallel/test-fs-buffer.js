'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

fs.access(Buffer.from(tmpdir.path), common.mustCall(assert.ifError));

const buf = Buffer.from(path.join(tmpdir.path, 'a.txt'));
fs.open(buf, 'w+', common.mustCall((err, fd) => {
  assert.ifError(err);
  assert(fd);
  fs.close(fd, common.mustCall(assert.ifError));
}));

assert.throws(
  () => {
    fs.accessSync(true);
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "path" argument must be of type string or an instance of ' +
             'Buffer or URL. Received type boolean (true)'
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
