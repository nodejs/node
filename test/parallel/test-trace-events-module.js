'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

if (process.argv[2] === 'child') {
  const trace_events = require('trace_events');
  // Note: this test is leveraging the fact that perf_hooks
  // will emit a very limited number of predictable trace events
  // that we can use to quickly verify that trace events are
  // enabled. Using the v8 and node.async_hooks targets tend
  // to be less predictable in terms of what is actually output
  // to the file. That said, if necessary, this test could
  // be modified to use any trace event output.
  const { performance } = require('perf_hooks');
  assert.deepStrictEqual(trace_events.getTracingCategories(), []);
  trace_events.enableTracingCategories('node.perf.usertiming');
  assert.deepStrictEqual(trace_events.getTracingCategories(),
                         ['node.perf.usertiming']);
  performance.mark('A');
  performance.mark('B');
  performance.measure('A to B', 'A', 'B');
  trace_events.disableTracingCategories('node.perf.usertiming');
  assert.deepStrictEqual(trace_events.getTracingCategories(), []);
} else {
  tmpdir.refresh();
  process.chdir(tmpdir.path);

  const expectedMarks = ['A', 'B'];
  const expectedBegins = [
    { cat: 'node,node.perf,node.perf.usertiming', name: 'A to B' }
  ];
  const expectedEnds = [
    { cat: 'node,node.perf,node.perf.usertiming', name: 'A to B' }
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
            assert.strictEqual(trace.cat,
                               'node,node.perf,node.perf.usertiming');
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
