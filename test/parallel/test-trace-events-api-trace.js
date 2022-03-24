'use strict';

const common = require('../common');
common.skipIfWorker(); // https://github.com/nodejs/node/issues/22767

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const cp = require('child_process');
const tmpdir = require('../common/tmpdir');

if (process.argv[2] === 'isChild') {
  const {
    createTracing,
    trace,
    events: {
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN: kBeforeEvent,
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_END: kEndEvent
    },
  } = require('trace_events');

  const tracing = createTracing({ categories: [ 'custom' ] });
  tracing.enable();
  trace(kBeforeEvent, 'custom', 'hello', 0, 'world');
  setTimeout(() => {
    trace(kEndEvent, 'custom', 'hello', 0, 'world');
    tracing.disable();
  }, 1);
} else {
  tmpdir.refresh();
  const parentDir = process.cwd();
  process.chdir(tmpdir.path);
  const proc = cp.fork(__filename, ['isChild'], {
    execArgv: [
      '--trace-event-categories',
      'custom',
    ]
  });

  proc.once('exit', common.mustCall(() => {
    const file = path.join(tmpdir.path, 'node_trace.1.log');
    assert(fs.existsSync(file));
    fs.readFile(file, { encoding: 'utf-8' }, common.mustSucceed((data) => {
      const traces = JSON.parse(data).traceEvents.filter((trace) => trace.cat !== '__metadata');
      assert.strictEqual(traces.length, 2);
      traces.forEach((trace) => {
        assert.strictEqual(trace.cat, 'custom');
        assert.strictEqual(trace.name, 'hello');
        assert.strictEqual(trace.args.data, 'world');
      });
    }));
    process.chdir(parentDir);
  }));
}
