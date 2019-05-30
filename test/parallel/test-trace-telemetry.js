'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');

const CODE =
  'const { telemetry } = require(\'internal/util/telemetry\');' +
  'telemetry(\'FOO\', \'%s %s %s\', 1, 2, 3);';

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const FILE_NAME = path.join(tmpdir.path, 'node_trace.1.log');

const proc = cp.spawn(process.execPath,
                      [ '--trace-event-categories', 'node.telemetry',
                        '--expose-internals',
                        '-e', CODE ],
                      { cwd: tmpdir.path });
proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    const traces = JSON.parse(data.toString()).traceEvents
      .filter((trace) => trace.cat === 'node,node.telemetry');
    assert.strictEqual(traces.length, 1);
    const trace = traces[0];
    assert.strictEqual(trace.name, 'FOO');
    assert.strictEqual(trace.args.data, '1 2 3');
  }));
}));
