'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

common.disableCrashOnUnhandledRejection();

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

if (process.argv[2] === 'child') {
  const p = Promise.reject(1);  // Handled later
  Promise.reject(2);  // Unhandled
  setImmediate(() => {
    p.catch(() => { /* intentional noop */ });
  });
} else {
  tmpdir.refresh();
  process.chdir(tmpdir.path);

  const proc = cp.fork(__filename,
                       [ 'child' ], {
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
