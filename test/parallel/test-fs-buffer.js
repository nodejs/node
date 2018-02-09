'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

fs.access(Buffer.from(tmpdir.path), common.mustCall((err) => {
  assert.ifError(err);
}));

const buf = Buffer.from(path.join(tmpdir.path, 'a.txt'));
fs.open(buf, 'w+', common.mustCall((err, fd) => {
  assert.ifError(err);
  assert(fd);
  fs.close(fd, common.mustCall((err) => {
    assert.ifError(err);
  }));
}));

assert.throws(() => {
  fs.accessSync(true);
}, /path must be a string or Buffer/);

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
