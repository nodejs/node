'use strict';

// This tests the behavior of loading a UMD module with --build-snapshot

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const file = fixtures.path('snapshot', 'marked.js');

{
  // By default, the snapshot blob path is snapshot.blob at cwd
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    file,
  ], {
    cwd: tmpdir.path
  });
  const stderr = child.stderr.toString();
  const stdout = child.stdout.toString();
  console.log(stderr);
  console.log(stdout);
  assert.strictEqual(child.status, 0);

  const stats = fs.statSync(tmpdir.resolve('snapshot.blob'));
  assert(stats.isFile());
}

{
  let child = spawnSync(process.execPath, [
    '--snapshot-blob',
    tmpdir.resolve('snapshot.blob'),
    fixtures.path('snapshot', 'check-marked.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
      NODE_TEST_USE_SNAPSHOT: 'true'
    }
  });
  let stderr = child.stderr.toString();
  const snapshotOutput = child.stdout.toString();
  console.log(stderr);
  console.log(snapshotOutput);

  assert.strictEqual(child.status, 0);
  assert(stderr.includes('NODE_TEST_USE_SNAPSHOT true'));

  child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    fixtures.path('snapshot', 'check-marked.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
      NODE_TEST_USE_SNAPSHOT: 'false'
    }
  });
  stderr = child.stderr.toString();
  const verifyOutput = child.stdout.toString();
  console.log(stderr);
  console.log(verifyOutput);

  assert.strictEqual(child.status, 0);
  assert(stderr.includes('NODE_TEST_USE_SNAPSHOT false'));

  assert(snapshotOutput.includes(verifyOutput));
}
