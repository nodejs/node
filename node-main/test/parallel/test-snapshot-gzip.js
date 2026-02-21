'use strict';

// This tests that a program that decompresses a gzip file and saves the
// content can be snapshotted and deserialized properly.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const file = fixtures.path('snapshot', 'decompress-gzip-sync.js');

{
  // By default, the snapshot blob path is snapshot.blob at cwd
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    file,
  ], {
    env: {
      ...process.env,
      NODE_TEST_FIXTURE: fixtures.path('person.jpg.gz'),
      NODE_TEST_MODE: 'snapshot'
    },
    cwd: tmpdir.path
  });
  const stderr = child.stderr.toString();
  const stdout = child.stdout.toString();
  console.log(stderr);
  console.log(stdout);
  assert.strictEqual(child.status, 0);

  const stats = fs.statSync(tmpdir.resolve('snapshot.blob'));
  assert(stats.isFile());
  assert(stdout.includes('NODE_TEST_MODE: snapshot'));
}

{
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    file,
  ], {
    env: {
      ...process.env,
      NODE_TEST_FIXTURE: fixtures.path('person.jpg.gz'),
      NODE_TEST_MODE: 'verify'
    },
    cwd: tmpdir.path
  });
  const stderr = child.stderr.toString();
  const stdout = child.stdout.toString();
  console.log(stderr);
  console.log(stdout);
  assert.strictEqual(child.status, 0);
  assert(stdout.includes('NODE_TEST_MODE: verify'));
}
