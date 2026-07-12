'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const testFilePath = path.join(tmpdir.path, 'fchown-test-file.txt');

fs.writeFileSync(testFilePath, 'test content for fchown');

{
  const fd = fs.openSync(testFilePath, 'r+');
  const stats = fs.fstatSync(fd);
  const uid = stats.uid;
  const gid = stats.gid;

  fs.fchown(fd, -1, -1, common.mustSucceed(() => {
    const stats = fs.fstatSync(fd);
    assert.strictEqual(stats.uid, uid);
    assert.strictEqual(stats.gid, gid);
    fs.closeSync(fd);
  }));
}

// Test sync fchown with -1 values
{
  const fd = fs.openSync(testFilePath, 'r+');
  const stats = fs.fstatSync(fd);
  const uid = stats.uid;
  const gid = stats.gid;

  fs.fchownSync(fd, -1, -1);
  const statsAfter = fs.fstatSync(fd);
  assert.strictEqual(statsAfter.uid, uid);
  assert.strictEqual(statsAfter.gid, gid);

  fs.closeSync(fd);
}
