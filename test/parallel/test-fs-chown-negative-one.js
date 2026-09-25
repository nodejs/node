'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const testFile = path.join(tmpdir.path, 'chown-test-file.txt');

fs.writeFileSync(testFile, 'test content for chown');

const stats = fs.statSync(testFile);
const uid = stats.uid;
const gid = stats.gid;

// -1 for uid and gid means "don't change the value"
{
  fs.chown(testFile, -1, -1, common.mustSucceed(() => {
    const stats = fs.statSync(testFile);
    assert.strictEqual(stats.uid, uid);
    assert.strictEqual(stats.gid, gid);
  }));
}
{
  fs.chownSync(testFile, -1, -1);
  const stats = fs.statSync(testFile);
  assert.strictEqual(stats.uid, uid);
  assert.strictEqual(stats.gid, gid);
}
