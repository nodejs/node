'use strict';

// This tests snapshot JS API

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const path = require('path');
const fs = require('fs');

const v8 = require('v8');

// By default it should be false. We'll test that it's true in snapshot
// building mode in the fixture.
assert(!v8.startupSnapshot.isBuildingSnapshot());

tmpdir.refresh();
const blobPath = path.join(tmpdir.path, 'snapshot.blob');
const entry = fixtures.path('snapshot', 'v8-startup-snapshot-api.js');
{
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    entry,
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
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
    }
  });

  const stdout = child.stdout.toString().trim();
  const file = fs.readFileSync(fixtures.path('x1024.txt'), 'utf8');
  assert.strictEqual(stdout, file);
  assert.strictEqual(child.status, 0);
}
