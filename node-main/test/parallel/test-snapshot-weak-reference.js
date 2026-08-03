'use strict';

// This tests that weak references work across serialization.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');

function runTest(entry) {
  console.log('running test with', entry);
  {
    const child = spawnSync(process.execPath, [
      '--expose-internals',
      '--expose-gc',
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
    const child = spawnSync(process.execPath, [
      '--expose-internals',
      '--expose-gc',
      '--snapshot-blob',
      blobPath,
    ], {
      cwd: tmpdir.path,
      env: {
        ...process.env,
      }
    });

    const stdout = child.stdout.toString().trim();
    const stderr = child.stderr.toString().trim();
    console.log(`[stdout]:\n${stdout}\n`);
    console.log(`[stderr]:\n${stderr}\n`);
    assert.strictEqual(child.status, 0);
  }
}

runTest(fixtures.path('snapshot', 'weak-reference.js'));
runTest(fixtures.path('snapshot', 'weak-reference-gc.js'));
