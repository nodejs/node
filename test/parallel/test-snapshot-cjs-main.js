'use strict';

// This tests that user land snapshots works when the instance restored from
// the snapshot is launched as a CJS module.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const file = fixtures.path('snapshot', 'mutate-fs.js');
const checkFile = fixtures.path('snapshot', 'check-mutate-fs.js');

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
  // Run the check file as a CJS module
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    checkFile,
  ], {
    cwd: tmpdir.path
  });

  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    assert.strictEqual(child.status, 0);
  }
}
