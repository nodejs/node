'use strict';

// Checks that the pprof time profiler cannot be started while building a
// startup snapshot.

require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('node:fs');

tmpdir.refresh();
const blobPath = tmpdir.resolve('snapshot.blob');
const entry = fixtures.path('snapshot', 'perf-hooks-pprof.js');
const child = spawnSync(process.execPath, [
  '--snapshot-blob',
  blobPath,
  '--build-snapshot',
  entry,
], {
  cwd: tmpdir.path,
});
const stderr = child.stderr.toString();
assert.match(
  stderr,
  /Starting the pprof time profiler is not supported in startup snapshot/);
assert.match(stderr, /ERR_NOT_SUPPORTED_IN_SNAPSHOT/);
assert.strictEqual(child.status, 1);
assert(!fs.existsSync(blobPath));
