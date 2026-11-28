// Flags: --expose-gc
'use strict';
require('../common');
const assert = require('assert');
const { GCProfiler } = require('v8');

// stop() removes the profiler's GC callbacks. Check that this leaves other
// running profilers alone, and that collecting a stopped profiler afterwards
// does not try to remove the callbacks again.
// Refs: https://github.com/nodejs/node/issues/67790

const first = new GCProfiler();
const second = new GCProfiler();
first.start();
second.start();
first.stop();

globalThis.gc();
const { statistics } = second.stop();
assert.ok(statistics.length > 0);

for (let i = 0; i < 10; i++) {
  const profiler = new GCProfiler();
  profiler.start();
  globalThis.gc();
  profiler.stop();
}
globalThis.gc();
