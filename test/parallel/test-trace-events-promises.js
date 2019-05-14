'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

common.disableCrashOnUnhandledRejection();

if (process.argv[2] === 'child') {
  const p = Promise.reject(1);  // Handled later
  Promise.reject(2);  // Unhandled
  setImmediate(() => {
    p.catch(() => { /* intentional noop */ });
  });
} else {
  tmpdir.refresh();

  const proc = cp.fork(__filename,
                       [ 'child' ], {
                         cwd: tmpdir.path,
                         execArgv: [
                           '--no-warnings',
                           '--trace-event-categories',
                           'node.promises.rejections'
                         ]
                       });

  proc.once('exit', common.mustCall(() => {
    const file = path.join(tmpdir.path, 'node_trace.1.log');

    assert(fs.existsSync(file));
    fs.readFile(file, common.mustCall((err, data) => {
      const traces = JSON.parse(data.toString()).traceEvents
        .filter((trace) => trace.cat !== '__metadata');
      traces.forEach((trace) => {
        assert.strictEqual(trace.pid, proc.pid);
        assert.strictEqual(trace.name, 'rejections');
        assert(trace.args.unhandled <= 2);
        assert(trace.args.handledAfter <= 1);
      });
    }));
  }));
}
