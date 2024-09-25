'use strict';

// This tests mutation to Error.stackTraceLimit in both the snapshot builder script
// and the snapshot main script work.

require('../common');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert, spawnSyncAndExitWithoutError } = require('../common/child_process');

const blobPath = tmpdir.resolve('snapshot.blob');

{
  tmpdir.refresh();
  // Check the mutation works without --stack-trace-limit.
  spawnSyncAndAssert(process.execPath, [
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    fixtures.path('snapshot', 'mutate-error-stack-trace-limit.js'),
  ], {
    cwd: tmpdir.path
  }, {
    stderr(output) {
      assert.match(output, /Error\.stackTraceLimit has been modified by the snapshot builder script/);
      assert.match(output, /It will be preserved after snapshot deserialization/);
    }
  });
  spawnSyncAndExitWithoutError(process.execPath, [
    '--snapshot-blob',
    blobPath,
  ], {
    cwd: tmpdir.path
  });
}

{
  tmpdir.refresh();
  // Check the mutation works with --stack-trace-limit.
  spawnSyncAndAssert(process.execPath, [
    '--stack-trace-limit=50',
    '--snapshot-blob',
    blobPath,
    '--build-snapshot',
    fixtures.path('snapshot', 'mutate-error-stack-trace-limit.js'),
  ], {
    cwd: tmpdir.path
  }, {
    stderr(output) {
      assert.match(output, /Error\.stackTraceLimit has been modified by the snapshot builder script/);
      assert.match(output, /It will be preserved after snapshot deserialization/);
    }
  });
  spawnSyncAndExitWithoutError(process.execPath, [
    // This checks that --stack-trace-limit=50 is ignored if the buidler script already mutated
    // Error.stackTraceLimit.
    '--stack-trace-limit=50',
    '--snapshot-blob',
    blobPath,
  ], {
    cwd: tmpdir.path
  });
}
