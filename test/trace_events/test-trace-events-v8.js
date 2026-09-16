'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

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
                      [ '--trace-events-enabled',
                        '--trace-event-categories', 'v8',
                        '-e', CODE ],
                      { cwd: tmpdir.path });

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  const traces = readTraceEvents(FILE_NAME);
  assert(traces.length > 0);
  // V8 trace events should be generated.
  assert(traces.some((trace) => {
    if (trace.pid !== proc.pid)
      return false;
    if (trace.cat !== 'v8')
      return false;
    if (!trace.name.startsWith('V8.'))
      return false;
    return true;
  }));

  // C++ async_hooks trace events should not be generated.
  assert(!traces.some((trace) => {
    if (trace.pid !== proc.pid)
      return false;
    if (trace.cat !== traceCategory('node.async_hooks'))
      return false;
    return true;
  }));


  // JavaScript async_hooks trace events should not be generated.
  assert(!traces.some((trace) => {
    if (trace.pid !== proc.pid)
      return false;
    if (trace.cat !== traceCategory('node.async_hooks'))
      return false;
    if (trace.name !== 'Timeout')
      return false;
    return true;
  }));
}));
