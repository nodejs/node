'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');

const CODE = `
  const { internalBinding } = require('internal/test/binding');
  const { trace } = internalBinding('trace_events');
  trace('b'.charCodeAt(0), 'custom',
        'type-value', 10, {'extra-value': 20 });
  trace('b'.charCodeAt(0), 'custom',
        'type-value', 20, {'first-value': 20, 'second-value': 30 });
  trace('b'.charCodeAt(0), 'custom', 'type-value', 30);
  trace('b'.charCodeAt(0), 'missing',
        'type-value', 10, {'extra-value': 20 });
`;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const FILE_NAME = path.join(tmpdir.path, 'node_trace.1.log');

const proc = cp.spawn(process.execPath,
                      [ '--trace-event-categories', 'custom',
                        '--no-warnings',
                        '--expose-internals',
                        '-e', CODE ],
                      { cwd: tmpdir.path });

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    const traces = JSON.parse(data.toString()).traceEvents
      .filter((trace) => trace.cat !== '__metadata');
    assert.strictEqual(traces.length, 3);

    assert.strictEqual(traces[0].pid, proc.pid);
    assert.strictEqual(traces[0].ph, 'b');
    assert.strictEqual(traces[0].cat, 'custom');
    assert.strictEqual(traces[0].name, 'type-value');
    assert.strictEqual(traces[0].id, '0xa');
    assert.deepStrictEqual(traces[0].args.data, { 'extra-value': 20 });

    assert.strictEqual(traces[1].pid, proc.pid);
    assert.strictEqual(traces[1].ph, 'b');
    assert.strictEqual(traces[1].cat, 'custom');
    assert.strictEqual(traces[1].name, 'type-value');
    assert.strictEqual(traces[1].id, '0x14');
    assert.deepStrictEqual(traces[1].args.data, {
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
