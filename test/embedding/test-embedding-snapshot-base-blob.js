'use strict';

// SnapshotConfig::base_blob: the embedder snapshot can be built on top of an
// existing V8 startup blob instead of a heap set up from scratch.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');
const {
  spawnSyncAndAssert,
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');

const embedtest = common.resolveBuiltBinary('embedtest');
const snapshotFixture = fixtures.path('snapshot', 'echo-args.js');
const v8Blob = tmpdir.resolve('v8.blob');
const nodeBlob = tmpdir.resolve('node-on-v8.blob');
const buildSnapshotExecArgs = [
  `eval(require("fs").readFileSync(${JSON.stringify(snapshotFixture)}, "utf8"))`,
  'arg1', 'arg2',
];

tmpdir.refresh();

spawnSyncAndExitWithoutError(embedtest, ['--', '--create-v8-startup-blob', v8Blob], { cwd: tmpdir.path });
assert.ok(fs.statSync(v8Blob).size > 0);

spawnSyncAndExitWithoutError(
  embedtest,
  ['--', ...buildSnapshotExecArgs, '--embedder-snapshot-blob', nodeBlob,
   '--embedder-snapshot-base-blob', v8Blob, '--embedder-snapshot-create'],
  { cwd: tmpdir.path });
assert.ok(fs.statSync(nodeBlob).size > fs.statSync(v8Blob).size);

spawnSyncAndAssert(
  embedtest,
  ['--', 'arg3', 'arg4', '--embedder-snapshot-blob', nodeBlob],
  { cwd: tmpdir.path },
  {
    stdout(output) {
      assert.deepStrictEqual(JSON.parse(output), {
        originalArgv: [embedtest, '__node_anonymous_main', ...buildSnapshotExecArgs],
        currentArgv: [embedtest, embedtest, 'arg3', 'arg4'],
      });
      return true;
    },
  });
