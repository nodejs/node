'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const path = require('path');
const fs = require('fs');

tmpdir.refresh();
const blobPath = path.join(tmpdir.path, 'my-snapshot.blob');
const file = fixtures.path('snapshot', 'mutate-fs.js');

{
  const child = spawnSync(process.execPath, [
    '--snapshot-main',
    file,
  ], {
    cwd: tmpdir.path
  });
  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    assert.strictEqual(child.status, 0);
  }
  const stats = fs.statSync(path.join(tmpdir.path, 'snapshot.blob'));
  assert(stats.isFile());
}

{
  let child = spawnSync(process.execPath, [
    '--snapshot-main',
    file,
    '--snapshot-blob',
    blobPath,
  ], {
    cwd: tmpdir.path
  });
  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    assert.strictEqual(child.status, 0);
  }
  const stats = fs.statSync(blobPath);
  assert(stats.isFile());

  child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '-p',
    'require("fs").foo',
  ], {
    cwd: tmpdir.path
  });

  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    assert.strictEqual(child.status, 0);
  }
  assert(/I am from the snapshot/.test(child.stdout.toString()));
}
