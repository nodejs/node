'use strict';

// This tests that user land snapshots works when the instance restored from
// the snapshot is launched with -p and -e

require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const file = fixtures.path('snapshot', 'cwd.js');

const subdir = path.join(tmpdir.path, 'foo');
fs.mkdirSync(subdir);

{
  // Create the snapshot.
  spawnSyncAndExitWithoutError(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    file,
  ], {
    cwd: tmpdir.path,
    encoding: 'utf8'
  }, {
    status: 0,
  });
}

{
  // Check a custom works.
  spawnSyncAndExitWithoutError(process.execPath, [
    '--snapshot-blob',
    blobPath,
    file,
  ], {
    cwd: subdir,
    encoding: 'utf8'
  }, {
    status: 0,
    trim: true,
    stdout: subdir,
  });
}
