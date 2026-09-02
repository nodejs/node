'use strict';

// Tests that an embedder can free the EmbedderSnapshotData it created an
// instance from and create a second instance from a fresh copy afterwards.

const common = require('../common');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const {
  spawnSyncAndAssert,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');

const embedtest = common.resolveBuiltBinary('embedtest');
const snapshotFixture = fixtures.path('snapshot', 'echo-args.js');
const blob = tmpdir.resolve('embedder-snapshot.blob');

tmpdir.refresh();

spawnSyncAndExitWithoutError(
  embedtest,
  [
    '--',
    `eval(require("fs").readFileSync(${JSON.stringify(snapshotFixture)}, "utf8"))`,
    'arg1', 'arg2', '--embedder-snapshot-blob', blob, '--embedder-snapshot-create',
  ],
  { cwd: tmpdir.path });

spawnSyncAndAssert(
  embedtest,
  ['--', 'arg3', '--embedder-snapshot-blob', blob, '--embedder-run-twice'],
  { cwd: tmpdir.path },
  {
    stdout(output) {
      assert.strictEqual(output.split('arg3').length, 3);
      return true;
    },
  });
