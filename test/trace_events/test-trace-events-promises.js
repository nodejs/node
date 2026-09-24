'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

const {
  defaultTraceFileName,
  readTraceEvents,
  checkTraceProcessor,
} = require('../common/trace_events');

checkTraceProcessor();

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
                           'node.promises.rejections',
                         ],
                       });

  proc.once('exit', common.mustCall(() => {
    const file = tmpdir.resolve(defaultTraceFileName);

    assert(fs.existsSync(file));
    const traces = readTraceEvents(file)
      .filter((trace) => trace.cat !== '__metadata');
    traces.forEach((trace) => {
      assert.strictEqual(trace.pid, proc.pid);
      assert.strictEqual(trace.name, 'rejections');
      assert(trace.args.unhandled <= 2);
      assert(trace.args.handledAfter <= 1);
    });
  }));
}
