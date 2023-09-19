'use strict';

// This tests that Node.js refuses to load snapshots built with incompatible
// V8 configurations.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const entry = fixtures.path('empty.js');

// The flag used can be any flag that makes a difference in
// v8::ScriptCompiler::CachedDataVersionTag(). --harmony
// is chosen here because it's stable enough and makes a difference.
{
  // Build a snapshot with --harmony.
  const child = spawnSync(process.execPath, [
    '--harmony',
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
  const stats = fs.statSync(tmpdir.resolve('snapshot.blob'));
  assert(stats.isFile());
}

{
  // Now load the snapshot without --harmony, which should fail.
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    blobPath,
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
    }
  });

  const stderr = child.stderr.toString().trim();
  assert.match(stderr, /Failed to load the startup snapshot/);
  assert.strictEqual(child.status, 14);
}

{
  // Load it again with --harmony and it should work.
  const child = spawnSync(process.execPath, [
    '--harmony',
    '--snapshot-blob',
    blobPath,
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
    }
  });

  if (child.status !== 0) {
    console.log(child.stderr.toString());
    console.log(child.stdout.toString());
    assert.strictEqual(child.status, 0);
  }
}
