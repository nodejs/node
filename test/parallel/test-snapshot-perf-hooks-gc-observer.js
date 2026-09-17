'use strict';

// Tests that 'gc' PerformanceObservers work after deserializing a snapshot
// that was built while a 'gc' PerformanceObserver was active, and that they
// can be disconnected without crashing.

require('../common');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const {
  spawnSyncAndAssert,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const entry = fixtures.path('snapshot', 'perf-hooks-gc-observer.js');

spawnSyncAndExitWithoutError(process.execPath, [
  '--expose-gc',
  '--snapshot-blob',
  blobPath,
  '--build-snapshot',
  entry,
], {
  cwd: tmpdir.path,
});

spawnSyncAndAssert(process.execPath, [
  '--expose-gc',
  '--snapshot-blob',
  blobPath,
], {
  cwd: tmpdir.path,
}, {
  stdout: 'ok',
  trim: true,
});
