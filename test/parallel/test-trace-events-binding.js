'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

const CODE = `
  process.binding("trace_events").emit(
    'b'.charCodeAt(0), 'custom',
    'type-value', 10, 'extra-value', 20);
  process.binding("trace_events").emit(
    'b'.charCodeAt(0), 'custom',
    'type-value', 20, 'first-value', 20, 'second-value', 30);
  process.binding("trace_events").emit(
    'b'.charCodeAt(0), 'custom',
    'type-value', 30);
  process.binding("trace_events").emit(
    'b'.charCodeAt(0), 'missing',
    'type-value', 10, 'extra-value', 20);
`;
const FILE_NAME = 'node_trace.1.log';

common.refreshTmpDir();
process.chdir(common.tmpDir);

const proc = cp.spawn(process.execPath,
                      [ '--trace-events-enabled',
                        '--trace-event-categories', 'custom',
                        '-e', CODE ]);

proc.once('exit', common.mustCall(() => {
  assert(common.fileExists(FILE_NAME));
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    const traces = JSON.parse(data.toString()).traceEvents;
    assert.strictEqual(traces.length, 3);

    assert.strictEqual(traces[0].pid, proc.pid);
    assert.strictEqual(traces[0].ph, 'b');
    assert.strictEqual(traces[0].cat, 'custom');
    assert.strictEqual(traces[0].name, 'type-value');
    assert.strictEqual(traces[0].id, '0xa');
    assert.deepStrictEqual(traces[0].args, { 'extra-value': 20 });

    assert.strictEqual(traces[1].pid, proc.pid);
    assert.strictEqual(traces[1].ph, 'b');
    assert.strictEqual(traces[1].cat, 'custom');
    assert.strictEqual(traces[1].name, 'type-value');
    assert.strictEqual(traces[1].id, '0x14');
    assert.deepStrictEqual(traces[1].args, {
      'first-value': 20,
      'second-value': 30
    });

    assert.strictEqual(traces[2].pid, proc.pid);
    assert.strictEqual(traces[2].ph, 'b');
    assert.strictEqual(traces[2].cat, 'custom');
    assert.strictEqual(traces[2].name, 'type-value');
    assert.strictEqual(traces[2].id, '0x1e');
    assert.deepStrictEqual(traces[2].args, { });
  }));
}));
