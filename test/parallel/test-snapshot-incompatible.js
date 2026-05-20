'use strict';

// This tests that Node.js refuses to load snapshots built with incompatible
// configurations (V8 or Node.js flags).

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const { internalBinding } = require('internal/test/binding');
const { getSnapshotAffectingFlags } = internalBinding('options');

tmpdir.refresh();
const entry = fixtures.path('empty.js');

// The flag used can be any flag that makes a difference in
// v8::ScriptCompiler::CachedDataVersionTag(). --harmony
// is chosen here because it's stable enough and makes a difference.
{
  // Build a snapshot with --harmony.
  const child = spawnSync(process.execPath, [
    '--harmony',
    '--snapshot-blob',
    tmpdir.resolve('snapshot.blob'),
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
  // Now load the snapshot without --harmony, which should fail if V8's
  // CachedDataVersionTag still distinguishes --harmony.  Some V8 versions no
  // longer treat --harmony as a cache-busting flag, so only assert when we
  // actually see the mismatch.
  const child = spawnSync(process.execPath, [
    '--snapshot-blob',
    tmpdir.resolve('snapshot.blob'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
    }
  });

  if (child.status !== 0) {
    const stderr = child.stderr.toString().trim();
    assert.match(stderr, /Failed to load the startup snapshot/);
    assert.strictEqual(child.status, 14);
  }
}

{
  // Load it again with --harmony and it should work.
  const child = spawnSync(process.execPath, [
    '--harmony',
    '--snapshot-blob',
    tmpdir.resolve('snapshot.blob'),
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

// Test that all snapshot-affecting boolean options are detected when mismatched.
//
// Flags that cannot be easily exercised in this test (e.g. require platform
// support that may be absent, or cause the snapshot build itself to fail).
const excludedFlags = new Set([
  // --experimental-quic is only available when built with QUIC support.
  '--experimental-quic',
]);

const snapshotFlags = getSnapshotAffectingFlags();

for (const { name, defaultIsTrue } of snapshotFlags) {
  if (excludedFlags.has(name)) continue;

  // For default_is_true options (e.g. --experimental-websocket), pass the
  // --no- form to disable the feature when building.
  const buildFlag = defaultIsTrue ? `--no-${name.slice(2)}` : name;
  const blobPath = tmpdir.resolve(
    `snapshot${name.replace(/[^a-zA-Z0-9]/g, '-')}.blob`
  );

  {
    const child = spawnSync(process.execPath, [
      buildFlag,
      '--snapshot-blob', blobPath,
      '--build-snapshot', entry,
    ], { cwd: tmpdir.path });
    if (child.status !== 0) {
      console.log(`Build with ${buildFlag} failed:`);
      console.log(child.stderr.toString());
      assert.strictEqual(child.status, 0);
    }
    assert(fs.statSync(blobPath).isFile());
  }

  {
    // Load without the build flag: should fail.
    const child = spawnSync(process.execPath, [
      '--snapshot-blob', blobPath,
    ], { cwd: tmpdir.path, env: { ...process.env } });

    const stderr = child.stderr.toString().trim();
    assert.match(
      stderr,
      /Failed to load the startup snapshot/,
      `Expected mismatch error for ${buildFlag}`
    );
    assert.match(
      stderr,
      new RegExp(name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')),
      `Expected flag name ${name} in error message`
    );
    assert.strictEqual(child.status, 14, `Expected exit 14 for ${buildFlag}`);
  }

  {
    // Load with the same flag: should succeed.
    const child = spawnSync(process.execPath, [
      buildFlag,
      '--snapshot-blob', blobPath,
    ], { cwd: tmpdir.path, env: { ...process.env } });

    if (child.status !== 0) {
      console.log(`Load with matching ${buildFlag} failed:`);
      console.log(child.stderr.toString());
      assert.strictEqual(child.status, 0);
    }
  }
}
