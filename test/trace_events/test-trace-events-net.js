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

checkTraceProcessor();

const CODE = `
  const net = require('net');
  const socket = net.connect('${common.PIPE}');
  socket.on('error', () => {});
  const server = net.createServer((socket) => {
    socket.destroy();
    server.close();
  }).listen(0, () => {
    net.connect(server.address().port);
  });
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
  for (const trace of traces) {
    if (trace.cat === traceCategory('node.net.native') &&
        trace.name === 'connect') {
      count++;
    }
  }
  // Two begin, two end
  assert.strictEqual(count, 4);
}));
