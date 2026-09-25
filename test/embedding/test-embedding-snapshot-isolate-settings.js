'use strict';

// IsolateSettings passed to NewIsolate() with a snapshot must survive
// CreateEnvironment(); see RunSnapshotWithIsolateSettings() in embedtest.cc.

const common = require('../common');
const tmpdir = require('../common/tmpdir');

const {
  spawnSyncAndAssert,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');

const embedtest = common.resolveBuiltBinary('embedtest');
const snapshotBlobArgs = [
  '--embedder-snapshot-blob', tmpdir.resolve('embedder-snapshot.blob'),
];
const buildSnapshotScript = `
  require('v8').startupSnapshot.setDeserializeMainFunction(() => {
    console.log(new Error('from the snapshot main function').stack);
  });
`;

tmpdir.refresh();

spawnSyncAndExitWithoutError(
  embedtest,
  ['--', buildSnapshotScript, ...snapshotBlobArgs, '--embedder-snapshot-create'],
  { cwd: tmpdir.path });

spawnSyncAndAssert(
  embedtest,
  ['--', ...snapshotBlobArgs, '--embedder-isolate-settings'],
  { cwd: tmpdir.path },
  {
    trim: true,
    stdout: 'stack trace prepared by the embedder',
  });
