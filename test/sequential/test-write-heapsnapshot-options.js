'use strict';

// Flags: --expose-internals

require('../common');

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
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
  const { path } = options;
  // If path is set, writeHeapSnapshot will write to the tmpdir.
  if (path) options.path = tmpdir.path;
  const filename = writeHeapSnapshot(undefined, options);
  // If path is set, the filename will be the provided path.
  if (path) assert(filename.includes(tmpdir.path));
  const snapshot = recordState(new ReadStream(filename));
  console.log('Snapshot nodes', snapshot.snapshot.length);
  console.log('Searching for', expected[0].children);
  tests.check(snapshot, expected);
  delete globalThis.obj;  // To pass the leaked global tests.
  return;
}

const { spawnSync } = require('child_process');
tmpdir.refresh();

// Start child processes to prevent the heap from growing too big.
for (let i = 0; i < tests.cases.length; ++i) {
  const child = spawnSync(
    process.execPath,
    ['--expose-internals', __filename, 'child', i + ''],
    {
      cwd: tmpdir.path,
    });
  const stderr = child.stderr.toString();
  const stdout = child.stdout.toString();
  console.log('[STDERR]', stderr);
  console.log('[STDOUT]', stdout);
  assert.strictEqual(child.status, 0);
}

{
  // Don't need to spawn child process for this test, it will fail inmediately.
  // Refs: https://github.com/nodejs/node/issues/58857
  for (const pathName of [null, true, false, {}, [], () => {}, 1, 0, NaN]) {
    assert.throws(() => {
      require('v8').writeHeapSnapshot(undefined, {
        path: pathName,
      });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
  }
}
