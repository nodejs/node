'use strict';

// Flags: --expose-internals

require('../common');

const { getHeapSnapshotOptionTests, recordState } = require('../common/heap');

const tests = getHeapSnapshotOptionTests();
if (process.argv[2] === 'child') {
  const { getHeapSnapshot } = require('v8');
  require(tests.fixtures);
  const { options, expected } = tests.cases[parseInt(process.argv[3])];
  const snapshot = recordState(getHeapSnapshot(options));
  console.log('Snapshot nodes', snapshot.snapshot.length);
  console.log('Searching for', expected[0].children);
  tests.check(snapshot, expected);
  delete globalThis.obj;  // To pass the leaked global tests.
  return;
}

const { spawnSync } = require('child_process');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

for (let i = 0; i < tests.cases.length; ++i) {
  const child = spawnSync(
    process.execPath,
    ['--expose-internals', __filename, 'child', i + ''],
    {
      cwd: tmpdir.path
    });
  const stderr = child.stderr.toString();
  const stdout = child.stdout.toString();
  console.log('[STDERR]', stderr);
  console.log('[STDOUT]', stdout);
  assert.strictEqual(child.status, 0);
}
