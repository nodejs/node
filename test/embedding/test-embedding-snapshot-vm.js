'use strict';

// Tests creating vm contexts after snapshot deserialization works
// in embedded environments.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');

const {
  spawnSyncAndAssert,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');

const snapshotFixture = fixtures.path('snapshot', 'create-vm.js');
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

// Build the snapshot with the vm-creating code in serialized main funciton.
spawnSyncAndExitWithoutError(
  embedtest,
  ['--', ...buildSnapshotExecArgs, ...snapshotBlobArgs, '--embedder-snapshot-create'],
  { cwd: tmpdir.path });

// Run the snapshot that creates a vm context from the snapshot.
spawnSyncAndAssert(
  embedtest,
  ['--', ...runSnapshotExecArgs, ...snapshotBlobArgs ],
  { cwd: tmpdir.path },
  {
    stdout: /value: 42/,
  });
