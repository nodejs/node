'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

if (process.argv[2] === 'child') {
  const trace_events = require('trace_events');
  const { performance } = require('perf_hooks');
  trace_events.setTracingCategories('node.perf.usertiming');
  performance.mark('A');
  performance.mark('B');
  performance.measure('A to B', 'A', 'B');
  trace_events.setTracingCategories(undefined);
} else {
  tmpdir.refresh();
  process.chdir(tmpdir.path);

  const expectedMarks = ['A', 'B'];
  const expectedBegins = [
    { cat: 'node.perf,node.perf.usertiming', name: 'A to B' }
  ];
  const expectedEnds = [
    { cat: 'node.perf,node.perf.usertiming', name: 'A to B' }
  ];

  const proc = cp.fork(__filename, [ 'child' ]);

  proc.once('exit', common.mustCall(() => {
    const file = path.join(tmpdir.path, 'node_trace.1.log');

    assert(common.fileExists(file));
    fs.readFile(file, common.mustCall((err, data) => {
      const traces = JSON.parse(data.toString()).traceEvents;
      assert.strictEqual(traces.length,
                         expectedMarks.length +
                         expectedBegins.length +
                         expectedEnds.length);

      traces.forEach((trace) => {
        assert.strictEqual(trace.pid, proc.pid);
        switch (trace.ph) {
          case 'R':
            assert.strictEqual(trace.cat, 'node.perf,node.perf.usertiming');
            assert.strictEqual(trace.name, expectedMarks.shift());
            break;
          case 'b':
            const expectedBegin = expectedBegins.shift();
            assert.strictEqual(trace.cat, expectedBegin.cat);
            assert.strictEqual(trace.name, expectedBegin.name);
            break;
          case 'e':
            const expectedEnd = expectedEnds.shift();
            assert.strictEqual(trace.cat, expectedEnd.cat);
            assert.strictEqual(trace.name, expectedEnd.name);
            break;
          default:
            assert.fail('Unexpected trace event phase');
        }
      });
    }));
  }));
}
