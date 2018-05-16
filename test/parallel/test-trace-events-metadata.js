'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

const CODE =
  'setTimeout(() => { for (var i = 0; i < 100000; i++) { "test" + i } }, 1)';
const FILE_NAME = 'node_trace.1.log';

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
process.chdir(tmpdir.path);

const proc = cp.spawn(process.execPath,
                      [ '--trace-events-enabled', '-e', CODE ]);
proc.once('exit', common.mustCall(() => {
  assert(common.fileExists(FILE_NAME));
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    const traces = JSON.parse(data.toString()).traceEvents;
    assert(traces.length > 0);
    assert(traces.some((trace) =>
      trace.cat === '__metadata' && trace.name === 'thread_name' &&
        trace.args.name === 'JavaScriptMainThread'));
    assert(traces.some((trace) =>
      trace.cat === '__metadata' && trace.name === 'thread_name' &&
        trace.args.name === 'BackgroundTaskRunner'));
  }));
}));
