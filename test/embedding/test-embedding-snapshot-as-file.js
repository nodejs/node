'use strict';

// Tests node::EmbedderSnapshotData::FromFile() works correctly.
// The snapshot is created in embedtest.cc.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const {
  spawnSyncAndAssert,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');

const snapshotFixture = fixtures.path('snapshot', 'echo-args.js');
const embedtest = common.resolveBuiltBinary('embedtest');

const buildSnapshotExecArgs = [
  `eval(require("fs").readFileSync(${JSON.stringify(snapshotFixture)}, "utf8"))`,
  'arg1', 'arg2',
];
const snapshotBlobArgs = [
  '--embedder-snapshot-blob', tmpdir.resolve('embedder-snapshot.blob'),
];

const runSnapshotExecArgs = ['arg3', 'arg4'];

tmpdir.refresh();

// Build the snapshot with the --embedder-snapshot-as-file flag.
spawnSyncAndExitWithoutError(
  embedtest,
  ['--', ...buildSnapshotExecArgs, ...snapshotBlobArgs, '--embedder-snapshot-as-file', '--embedder-snapshot-create'],
  { cwd: tmpdir.path });

// Run the snapshot and check the serialized and refreshed argv values.
spawnSyncAndAssert(
  embedtest,
  ['--', ...runSnapshotExecArgs, ...snapshotBlobArgs ],
  { cwd: tmpdir.path },
  {
    stdout(output) {
      assert.deepStrictEqual(JSON.parse(output), {
        originalArgv: [embedtest, '__node_anonymous_main', ...buildSnapshotExecArgs],
        currentArgv: [embedtest, ...runSnapshotExecArgs],
      });
      return true;
    },
  });
