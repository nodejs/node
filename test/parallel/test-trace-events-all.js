'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

const CODE =
  'setTimeout(() => { for (var i = 0; i < 100000; i++) { "test" + i } }, 1)';
const FILE_NAME = 'node_trace.1.log';

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
process.chdir(tmpdir.path);

const proc = cp.spawn(process.execPath,
                      [ '--trace-events-enabled', '-e', CODE ]);

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    const traces = JSON.parse(data.toString()).traceEvents;
    assert(traces.length > 0);
    // V8 trace events should be generated.
    assert(traces.some((trace) => {
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
      if (trace.cat !== 'node,node.async_hooks')
        return false;
      return true;
    }));


    // JavaScript async_hooks trace events should be generated.
    assert(traces.some((trace) => {
      if (trace.pid !== proc.pid)
        return false;
      if (trace.cat !== 'node,node.async_hooks')
        return false;
      if (trace.name !== 'Timeout')
        return false;
      return true;
    }));
  }));
}));
