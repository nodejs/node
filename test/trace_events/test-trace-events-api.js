// Flags: --expose-gc --no-warnings --expose-internals
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  // https://github.com/nodejs/node/issues/22767
  common.skip('This test only works on a main thread');
}

try {
  require('trace_events');
} catch {
  common.skip('missing trace events');
}

const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const {
  defaultTraceFileName,
  readTraceEvents,
  checkTraceProcessor,
} = require('../common/trace_events');
const {
  createTracing,
  getEnabledCategories,
} = require('trace_events');

checkTraceProcessor();

function getEnabledCategoriesFromCommandLine() {
  const indexOfCatFlag = process.execArgv.indexOf('--trace-event-categories');
  if (indexOfCatFlag === -1) {
    return undefined;
  }
  return process.execArgv[indexOfCatFlag + 1];
}

const isChild = process.argv[2] === 'child';
const enabledCategories = getEnabledCategoriesFromCommandLine();

assert.strictEqual(getEnabledCategories(), enabledCategories);
for (const i of [1, 'foo', true, false, null, undefined]) {
  assert.throws(() => createTracing(i), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
  assert.throws(() => createTracing({ categories: i }), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
}

assert.throws(
  () => createTracing({ categories: [] }),
  {
    code: 'ERR_TRACE_EVENTS_CATEGORY_REQUIRED',
    name: 'TypeError',
  },
);

const tracing = createTracing({ categories: [ 'node.perf' ] });

assert.strictEqual(tracing.categories, 'node.perf');
assert.strictEqual(tracing.enabled, false);

assert.strictEqual(getEnabledCategories(), enabledCategories);
tracing.enable();
tracing.enable();  // Purposefully enable twice to test calling twice
assert.strictEqual(tracing.enabled, true);

assert.strictEqual(getEnabledCategories(),
                   [
                     ...[enabledCategories].filter((_) => !!_), 'node.perf',
                   ].join(','));

tracing.disable();
assert.strictEqual(tracing.enabled, false);

const tracing2 = createTracing({ categories: [ 'foo' ] });
tracing2.enable();
assert.strictEqual(getEnabledCategories(), 'foo');

tracing2.disable();
tracing2.disable();  // Purposefully disable twice to test calling twice
assert.strictEqual(getEnabledCategories(), enabledCategories);

if (isChild) {
  // Perfetto only accepts the synchronous begin/end phases, so take the phase
  // constants from internal/trace_events, which picks the right pair.
  const {
    trace,
    kAsyncBegin,
    kAsyncEnd,
  } = require('internal/trace_events');

  tracing.enable();

  trace(kAsyncBegin, 'foo', 'test1', 0, 'test');
  setTimeout(() => {
    trace(kAsyncEnd, 'foo', 'test1');
  }, 1);
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
    globalThis.gc();
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

  testApiInChildProcess(['--trace-event-categories', 'foo'], () => {
    testApiInChildProcess(['--trace-event-categories', 'foo']);
  });
}

function testApiInChildProcess(execArgs, cb) {
  tmpdir.refresh();
  // Save the current directory so we can chdir back to it later
  const parentDir = process.cwd();
  process.chdir(tmpdir.path);

  // The child emits one begin/end pair. Perfetto merges a pair into a single
  // complete event when the trace is converted back.
  const expectedPhases = common.hasPerfetto ? ['X'] : ['b', 'e'];

  const proc = cp.fork(__filename,
                       ['child'],
                       {
                         execArgv: [
                           '--expose-gc',
                           '--expose-internals',
                           '--no-warnings',
                           ...execArgs,
                         ],
                       });

  proc.once('exit', common.mustCall(() => {
    const file = tmpdir.resolve(defaultTraceFileName);
    assert(fs.existsSync(file));

    const traces = readTraceEvents(file)
      .filter((trace) => trace.cat !== '__metadata');

    assert.deepStrictEqual(traces.map((trace) => trace.ph), expectedPhases);
    for (const trace of traces) {
      assert.strictEqual(trace.pid, proc.pid);
      assert.strictEqual(trace.cat, 'foo');
      assert.strictEqual(trace.name, 'test1');
    }

    process.chdir(parentDir);
    cb && process.nextTick(cb);
  }));
}
