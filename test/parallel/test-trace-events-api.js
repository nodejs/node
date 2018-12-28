// Flags: --expose-gc --no-warnings
'use strict';

const common = require('../common');

if (!process.binding('config').hasTracing)
  common.skip('missing trace events');
common.skipIfWorker(); // https://github.com/nodejs/node/issues/22767

const assert = require('assert');
const cp = require('child_process');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const {
  createTracing,
  getEnabledCategories
} = require('trace_events');

const isChild = process.argv[2] === 'child';
const enabledCategories = isChild ? 'foo' : undefined;

assert.strictEqual(getEnabledCategories(), enabledCategories);
[1, 'foo', true, false, null, undefined].forEach((i) => {
  common.expectsError(() => createTracing(i), {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });
  common.expectsError(() => createTracing({ categories: i }), {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });
});

common.expectsError(
  () => createTracing({ categories: [] }),
  {
    code: 'ERR_TRACE_EVENTS_CATEGORY_REQUIRED',
    type: TypeError
  }
);

const tracing = createTracing({ categories: [ 'node.perf' ] });

assert.strictEqual(tracing.categories, 'node.perf');
assert.strictEqual(tracing.enabled, false);

assert.strictEqual(getEnabledCategories(), enabledCategories);
tracing.enable();
tracing.enable();  // purposefully enable twice to test calling twice
assert.strictEqual(tracing.enabled, true);

assert.strictEqual(getEnabledCategories(),
                   isChild ? 'foo,node.perf' : 'node.perf');

tracing.disable();
assert.strictEqual(tracing.enabled, false);

const tracing2 = createTracing({ categories: [ 'foo' ] });
tracing2.enable();
assert.strictEqual(getEnabledCategories(), 'foo');

tracing2.disable();
tracing2.disable();  // purposefully disable twice to test calling twice
assert.strictEqual(getEnabledCategories(), enabledCategories);

if (isChild) {
  tracing.enable();
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

  // Test that enabled tracing references do not get garbage collected
  // until after they are disabled.
  {
    {
      let tracing3 = createTracing({ categories: [ 'abc' ] });
      tracing3.enable();
      assert.strictEqual(getEnabledCategories(), 'abc');
      tracing3 = undefined;
    }
    global.gc();
    assert.strictEqual(getEnabledCategories(), 'abc');
    // Not able to disable the thing after this point, however.
  }

  {
    common.expectWarning(
      'Warning',
      'Possible trace_events memory leak detected. There are more than ' +
      '10 enabled Tracing objects.');
    for (let n = 0; n < 10; n++) {
      const tracing = createTracing({ categories: [ `a${n}` ] });
      tracing.enable();
    }
  }

  tmpdir.refresh();
  process.chdir(tmpdir.path);

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
                       ['child'],
                       { execArgv: [ '--expose-gc',
                                     '--trace-event-categories',
                                     'foo' ] });

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
