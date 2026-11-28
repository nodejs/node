'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const testFile = tmpdir.resolve('lchown-test-file.txt');
const testLink = tmpdir.resolve('lchown-test-link');

fs.writeFileSync(testFile, 'test content for lchown');
fs.symlinkSync(testFile, testLink);

const stats = fs.lstatSync(testLink);
const uid = stats.uid;
const gid = stats.gid;

// -1 for uid and gid means "don't change the value"
{
  fs.lchown(testLink, -1, -1, common.mustSucceed(() => {
    const stats = fs.lstatSync(testLink);
    assert.strictEqual(stats.uid, uid);
    assert.strictEqual(stats.gid, gid);
  }));
}
{
  fs.lchownSync(testLink, -1, -1);
  const stats = fs.lstatSync(testLink);
  assert.strictEqual(stats.uid, uid);
  assert.strictEqual(stats.gid, gid);
}
