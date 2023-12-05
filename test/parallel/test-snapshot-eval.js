'use strict';

// This tests that user land snapshots works when the instance restored from
// the snapshot is launched with -p and -e

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const file = fixtures.path('snapshot', 'mutate-fs.js');

{
  // Create the snapshot.
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    file,
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
}

{
  // Check -p works.
  const child = spawnSync(process.execPath, [
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

{
  // Check -e works.
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '-e',
    'console.log(require("fs").foo)',
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
