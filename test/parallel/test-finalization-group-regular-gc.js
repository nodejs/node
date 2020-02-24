// Flags: --harmony-weak-refs
'use strict';
require('../common');
const assert = require('assert');

// Test that finalization callbacks do not crash when caused through a regular
// GC (not global.gc()).

const start = Date.now();
const g = new globalThis.FinalizationRegistry(() => {
  const diff = Date.now() - start;
  assert(diff < 10000, `${diff} >= 10000`);
});
g.register({}, 42);

setImmediate(() => {
  const arr = [];
  // Build up enough memory usage to hopefully trigger a platform task but not
  // enough to trigger GC as an interrupt.
  while (arr.length < 1000000) arr.push([]);

  setTimeout(() => {
    g;  // Keep reference alive.
  }, 200000).unref();
});
