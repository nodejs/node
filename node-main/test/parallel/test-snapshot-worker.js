'use strict';

// This tests snapshot JS API using the example in the docs.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const entry = fixtures.path('snapshot', 'worker.js');
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
  assert.match(
    stderr,
    /Error: Creating workers is not supported in startup snapshot/);
  assert.match(
    stderr,
    /ERR_NOT_SUPPORTED_IN_SNAPSHOT/);
  assert.strictEqual(child.status, 1);
  assert(!fs.existsSync(blobPath));
}
