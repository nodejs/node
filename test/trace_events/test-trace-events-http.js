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
  const http = require('http');
  const server = http.createServer((req, res) => {
    res.end('ok');
    server.close();
  }).listen(0, () => {
    http.get({port: server.address().port});
  });
`;

tmpdir.refresh();
const FILE_NAME = tmpdir.resolve(defaultTraceFileName);

const proc = cp.spawn(process.execPath,
                      [ '--trace-events-enabled',
                        '--trace-event-categories', 'node.http',
                        '-e', CODE ],
                      { cwd: tmpdir.path });

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  const traces = readTraceEvents(FILE_NAME);
  assert(traces.length > 0);
  let count = 0;
  for (const trace of traces) {
    if (trace.cat === traceCategory('node.http') &&
        ['http.server.request', 'http.client.request'].includes(trace.name)) {
      count++;
    }
  }
  // Two begin and two end, which perfetto records as two complete events.
  assert.strictEqual(count, common.hasPerfetto ? 2 : 4);
}));
