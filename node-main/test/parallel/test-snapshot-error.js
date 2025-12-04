'use strict';

// This tests that the errors in the snapshot script can be handled
// properly.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const entry = fixtures.path('snapshot', 'error.js');

// --build-snapshot should be run with an entry point.
{
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
  ], {
    cwd: tmpdir.path
  });
  const stderr = child.stderr.toString();
  console.log(child.status);
  console.log(stderr);
  console.log(child.stdout.toString());
  assert.strictEqual(child.status, 9);
  assert.match(stderr,
               /--build-snapshot must be used with an entry point script/);
  assert(!fs.existsSync(tmpdir.resolve('snapshot.blob')));
}

// Loading a non-existent snapshot should fail.
{
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    entry,
  ], {
    cwd: tmpdir.path
  });
  const stderr = child.stderr.toString();
  console.log(child.status);
  console.log(stderr);
  console.log(child.stdout.toString());
  assert.strictEqual(child.status, 14);
  assert.match(stderr, /Cannot open/);
  assert(!fs.existsSync(tmpdir.resolve('snapshot.blob')));
}


// Running an script that throws an error should result in an exit code of 1.
{
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    entry,
  ], {
    cwd: tmpdir.path
  });
  const stderr = child.stderr.toString();
  console.log(child.status);
  console.log(stderr);
  console.log(child.stdout.toString());
  assert.strictEqual(child.status, 1);
  assert.match(stderr, /error\.js:1/);
  assert(!fs.existsSync(tmpdir.resolve('snapshot.blob')));
}
