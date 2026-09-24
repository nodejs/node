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
  traceCategory,
} = require('../common/trace_events');

if (!common.isLinux) common.skip();
checkTraceProcessor();

const CODE = `
  const net = require('net');
  net.connect('${common.PIPE}').on('error', () => {});
  net.connect('\\0${common.PIPE}').on('error', () => {});
`;

tmpdir.refresh();
const FILE_NAME = tmpdir.resolve(defaultTraceFileName);

const proc = cp.spawn(process.execPath,
                      [ '--trace-events-enabled',
                        '--trace-event-categories', 'node.net.native',
                        '-e', CODE ],
                      { cwd: tmpdir.path });

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  const traces = readTraceEvents(FILE_NAME);
  assert(traces.length > 0);
  let count = 0;
  traces.forEach((trace) => {
    if (trace.cat === traceCategory('node.net.native') &&
        trace.name === 'connect') {
      count++;
      if (trace.ph === 'b') {
        assert.ok(!!trace.args.path_type);
        assert.ok(!!trace.args.pipe_path);
      }
    }
  });
  assert.strictEqual(count, 4);
}));
