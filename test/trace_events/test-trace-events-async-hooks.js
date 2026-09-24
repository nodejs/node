'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const util = require('util');

const {
  defaultTraceFileName,
  readTraceEvents,
  checkTraceProcessor,
  traceCategory,
} = require('../common/trace_events');

checkTraceProcessor();

const CODE =
  'setTimeout(() => { for (let i = 0; i < 100000; i++) { "test" + i } }, 1)';

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const FILE_NAME = tmpdir.resolve(defaultTraceFileName);

const proc = cp.spawn(process.execPath,
                      [ '--trace-event-categories', 'node.async_hooks',
                        '-e', CODE ],
                      { cwd: tmpdir.path });

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  const traces = readTraceEvents(FILE_NAME);
  assert(traces.length > 0);
  // V8 trace events should not be generated.
  assert(!traces.some((trace) => {
    if (trace.pid !== proc.pid)
      return false;
    if (trace.cat !== 'v8')
      return false;
    if (trace.name !== 'V8.ScriptCompiler')
      return false;
    return true;
  }));

  // C++ async_hooks trace events should be generated.
  assert(traces.some((trace) => {
    if (trace.pid !== proc.pid)
      return false;
    if (trace.cat !== traceCategory('node.async_hooks'))
      return false;
    return true;
  }));

  // JavaScript async_hooks trace events should be generated.
  assert(traces.some((trace) => {
    if (trace.pid !== proc.pid)
      return false;
    if (trace.cat !== traceCategory('node.async_hooks'))
      return false;
    if (trace.name !== 'Timeout')
      return false;
    return true;
  }));

  // Check args in init events. Perfetto records the begin/end pair of an
  // async_hooks event as one complete event.
  const initPhase = common.hasPerfetto ? 'X' : 'b';
  const initEvents = traces.filter((trace) => {
    return (trace.ph === initPhase && !trace.name.includes('_CALLBACK'));
  });
  assert.ok(initEvents.every((trace) => {
    return (trace.args.data.executionAsyncId > 0 &&
            trace.args.data.triggerAsyncId > 0);
  }), `Unexpected initEvents format: ${util.inspect(initEvents)}`);
}));
