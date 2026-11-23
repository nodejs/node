'use strict';

const common = require('../common');
const assert = require('node:assert');

common.expectWarning(
  'ExperimentalWarning',
  'perf_hooks/pprof is an experimental feature and might change at any time');

assert.ok(!process.moduleLoadList.includes('NativeModule perf_hooks/pprof'));
// Loading node:perf_hooks does not load node:perf_hooks/pprof.
require('node:perf_hooks');
assert.ok(!process.moduleLoadList.includes('NativeModule perf_hooks/pprof'));

const pprof = require('node:perf_hooks/pprof');
assert.strictEqual(require('perf_hooks/pprof'), pprof);
assert.strictEqual(process.getBuiltinModule('node:perf_hooks/pprof'), pprof);

assert.deepStrictEqual(Object.keys(pprof).sort(), [
  'encode',
  'encodeSync',
  'time',
]);
assert.strictEqual(typeof pprof.encode, 'function');
assert.strictEqual(typeof pprof.encodeSync, 'function');

const { time } = pprof;
assert.ok(Object.isFrozen(time));
assert.deepStrictEqual(Object.keys(time).sort(), [
  'constants',
  'isStarted',
  'profile',
  'start',
  'stop',
  'v8ProfilerStuckEventLoopDetected',
]);
assert.ok(Object.isFrozen(time.constants));
assert.deepStrictEqual({ ...time.constants }, {
  GARBAGE_COLLECTION_FUNCTION_NAME: 'Garbage Collection',
  NON_JS_THREADS_FUNCTION_NAME: 'Non JS threads activity',
});
assert.strictEqual(time.isStarted(), false);
assert.strictEqual(time.v8ProfilerStuckEventLoopDetected(), 0);

import('node:perf_hooks/pprof').then(common.mustCall((ns) => {
  assert.strictEqual(ns.default, pprof);
  assert.strictEqual(ns.time, time);
}));
