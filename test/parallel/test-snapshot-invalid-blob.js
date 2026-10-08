'use strict';

// This tests that Node.js reports an error, rather than crashing, when the
// file passed to --snapshot-blob is empty, is not a snapshot, or is truncated.

require('../common');
const {
  spawnSyncAndExit,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
const entry = fixtures.path('empty.js');

function expectFailure(blobPath, stderr) {
  spawnSyncAndExit(process.execPath, ['--snapshot-blob', blobPath, entry], {
    cwd: tmpdir.path,
  }, {
    status: 14,
    signal: null,
    stderr,
  });
}

{
  const blobPath = tmpdir.resolve('empty.blob');
  fs.writeFileSync(blobPath, '');
  expectFailure(blobPath, /not a Node\.js snapshot blob/);
}

{
  const blobPath = tmpdir.resolve('garbage.blob');
  fs.writeFileSync(blobPath, Buffer.alloc(4096, 0x61));
  expectFailure(blobPath, /not a Node\.js snapshot blob/);
}

{
  const blobPath = tmpdir.resolve('snapshot.blob');
  spawnSyncAndExitWithoutError(process.execPath, [
    '--snapshot-blob', blobPath, '--build-snapshot', entry,
  ], { cwd: tmpdir.path });
  const blob = fs.readFileSync(blobPath);
  const truncatedPath = tmpdir.resolve('truncated.blob');
  fs.writeFileSync(truncatedPath, blob.subarray(0, blob.length >> 1));
  expectFailure(truncatedPath, /truncated/);
}
