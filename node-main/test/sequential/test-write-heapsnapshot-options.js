'use strict';

// Flags: --expose-internals

require('../common');

const fs = require('fs');
const { getHeapSnapshotOptionTests, recordState } = require('../common/heap');

class ReadStream {
  constructor(filename) {
    this._content = fs.readFileSync(filename, 'utf-8');
  }
  pause() {}
  read() { return this._content; }
}

const tests = getHeapSnapshotOptionTests();
if (process.argv[2] === 'child') {
  const { writeHeapSnapshot } = require('v8');
  require(tests.fixtures);
  const { options, expected } = tests.cases[parseInt(process.argv[3])];
  const filename = writeHeapSnapshot(undefined, options);
  const snapshot = recordState(new ReadStream(filename));
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

// Start child processes to prevent the heap from growing too big.
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
