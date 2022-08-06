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

let snapshotScript = 'node:embedded_snapshot_main';
if (!process.config.variables.node_use_node_snapshot) {
  // Check that Node.js built without an embedded snapshot
  // exits with 1 when node:embedded_snapshot_main is specified
  // as snapshot entry point.
  const child = spawnSync(process.execPath, [
    '--build-snapshot',
    snapshotScript,
  ], {
    cwd: tmpdir.path
  });

  assert.match(
    child.stderr.toString(),
    /Node\.js was built without embedded snapshot/);
  assert.strictEqual(child.status, 1);

  snapshotScript = fixtures.path('empty.js');
}

// By default, the snapshot blob path is cwd/snapshot.blob.
{
  // Create the snapshot.
  const child = spawnSync(process.execPath, [
    '--build-snapshot',
    snapshotScript,
  ], {
    cwd: tmpdir.path
  });
  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    console.log(child.signal);
    assert.strictEqual(child.status, 0);
  }
  const stats = fs.statSync(path.join(tmpdir.path, 'snapshot.blob'));
  assert(stats.isFile());
}

tmpdir.refresh();
const blobPath = path.join(tmpdir.path, 'my-snapshot.blob');
{
  // Create the snapshot.
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    snapshotScript,
  ], {
    cwd: tmpdir.path
  });
  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    console.log(child.signal);
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
    console.log(child.signal);
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
    fixtures.path('snapshot', 'marked.js'),
  ], {
    cwd: tmpdir.path
  });

  // Check that it is a noop.
  assert.strictEqual(child.stdout.toString().trim(), '');
  assert.strictEqual(child.stderr.toString().trim(), '');
  assert.strictEqual(child.status, 0);
}
