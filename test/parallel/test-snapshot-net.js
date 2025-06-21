'use strict';

// This tests that a local TCP server can be snapshotted and the
// diagnostics channels work across serialization.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const entry = fixtures.path('snapshot', 'server.js');
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
  const stats = fs.statSync(tmpdir.resolve('snapshot.blob'));
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
  console.log(`[stdout]:\n${stdout}\n`);
  const stderr = child.stderr.toString().trim();
  console.log(`[stderr]:\n${stderr}\n`);
  assert.strictEqual(child.status, 0);

  const lines = stdout.split('\n');
  assert.strictEqual(lines.length, 4);

  // The log should look like this:
  // server port ${port}
  // From client diagnostics channel
  // From server diagnostics channel: ${port}
  // recv.length: 256
  assert.match(lines[0], /server port (\d+)/);
  const port = lines[0].match(/server port (\d+)/)[1];
  assert.match(lines[1], /From client diagnostics channel/);
  assert.match(lines[2], new RegExp(`From server diagnostics channel: ${port}`));
  assert.match(lines[3], /recv\.length: 256/);
}
