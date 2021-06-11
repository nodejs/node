'use strict';

// This tests that user land snapshots works when the instance restored from
// the snapshot is launched with --help, --check

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
  // Create the snapshot.
  const child = spawnSync(process.execPath, [
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
}

{
  // Check --help.
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--help',
  ], {
    cwd: tmpdir.path
  });

  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    assert.strictEqual(child.status, 0);
  }

  assert(child.stdout.toString().includes('--help'));
}

{
  // Check -c.
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '-c',
    file,
  ], {
    cwd: tmpdir.path
  });

  // Check that it is a noop.
  assert.strictEqual(child.stdout.toString().trim(), '');
  assert.strictEqual(child.stderr.toString().trim(), '');
  assert.strictEqual(child.status, 0);
}
