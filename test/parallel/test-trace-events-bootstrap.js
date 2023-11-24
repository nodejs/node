'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

const names = [
  'environment',
  'nodeStart',
  'v8Start',
  'loopStart',
  'loopExit',
  'bootstrapComplete',
];

if (process.argv[2] === 'child') {
  1 + 1; // eslint-disable-line no-unused-expressions
} else {
  tmpdir.refresh();

  const proc = cp.fork(__filename,
                       [ 'child' ], {
                         cwd: tmpdir.path,
                         execArgv: [
                           '--trace-event-categories',
                           'node.bootstrap',
                         ]
                       });

  proc.once('exit', common.mustCall(() => {
    const file = tmpdir.resolve('node_trace.1.log');

    assert(fs.existsSync(file));
    fs.readFile(file, common.mustSucceed((data) => {
      const traces = JSON.parse(data.toString()).traceEvents
        .filter((trace) => trace.cat !== '__metadata');
      traces.forEach((trace) => {
        assert.strictEqual(trace.pid, proc.pid);
        assert(names.includes(trace.name));
      });
    }));
  }));
}
