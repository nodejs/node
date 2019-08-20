'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

if (process.argv[2] === 'child') {
  const { performance } = require('perf_hooks');

  // Will emit mark and measure trace events
  performance.mark('A');
  setTimeout(() => {
    performance.mark('B');
    performance.measure('A to B', 'A', 'B');
  }, 1);

  // Intentional non-op, part of the test
  function f() {}
  const ff = performance.timerify(f);
  ff();  // Will emit a timerify trace event
} else {
  tmpdir.refresh();

  const expectedMarks = ['A', 'B'];
  const expectedBegins = [
    { cat: 'node,node.perf,node.perf.timerify', name: 'f' },
    { cat: 'node,node.perf,node.perf.usertiming', name: 'A to B' }
  ];
  const expectedEnds = [
    { cat: 'node,node.perf,node.perf.timerify', name: 'f' },
    { cat: 'node,node.perf,node.perf.usertiming', name: 'A to B' }
  ];

  const proc = cp.fork(__filename,
                       [
                         'child'
                       ], {
                         cwd: tmpdir.path,
                         execArgv: [
                           '--trace-event-categories',
                           'node.perf'
                         ]
                       });

  proc.once('exit', common.mustCall(() => {
    const file = path.join(tmpdir.path, 'node_trace.1.log');

    assert(fs.existsSync(file));
    fs.readFile(file, common.mustCall((err, data) => {
      const traces = JSON.parse(data.toString()).traceEvents
        .filter((trace) => trace.cat !== '__metadata');
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
            assert.strictEqual(trace.name,
                               expectedMarks.shift());
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
