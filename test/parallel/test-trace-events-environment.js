// Flags: --no-warnings

'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

// This tests the emission of node.environment trace events

const names = new Set([
  'Environment',
  'RunAndClearNativeImmediates',
  'CheckImmediate',
  'RunTimers',
  'BeforeExit',
  'RunCleanup',
  'AtExit',
]);

if (process.argv[2] === 'child') {
  /* eslint-disable no-unused-expressions */
  // This is just so that the child has something to do.
  1 + 1;
  // These ensure that the RunTimers, CheckImmediate, and
  // RunAndClearNativeImmediates appear in the list.
  setImmediate(() => { 1 + 1; });
  setTimeout(() => { 1 + 1; }, 1);
  /* eslint-enable no-unused-expressions */
} else {
  tmpdir.refresh();

  const proc = cp.fork(__filename,
                       [ 'child' ], {
                         cwd: tmpdir.path,
                         execArgv: [
                           '--trace-event-categories',
                           'node.environment',
                         ]
                       });

  proc.once('exit', common.mustCall(async () => {
    const file = tmpdir.resolve('node_trace.1.log');
    const checkSet = new Set();

    assert(fs.existsSync(file));
    const data = await fs.promises.readFile(file);
    JSON.parse(data.toString()).traceEvents
      .filter((trace) => trace.cat !== '__metadata')
      .forEach((trace) => {
        assert.strictEqual(trace.pid, proc.pid);
        assert(names.has(trace.name));
        checkSet.add(trace.name);
      });

    assert.deepStrictEqual(names, checkSet);
  }));
}
