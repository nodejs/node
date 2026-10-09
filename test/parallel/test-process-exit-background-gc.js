// Flags: --stress-concurrent-allocation
'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }

// Regression test for https://github.com/nodejs/node/issues/64274.
// process.exit() joined the V8 platform workers while one was parked
// waiting for a GC that the main thread no longer performs.

const { pbkdf2, pbkdf2Sync } = require('crypto');

// A small live set keeps the old generation limit low.
const keep = [];
for (let i = 0; i < 1e5; i++) keep.push({ i });

// Pick an iteration count that keeps the threadpool busy for about a second.
const probeIterations = 2e5;
const start = process.hrtime.bigint();
pbkdf2Sync('a', 'b', probeIterations, 64, 'sha512');
const nsPerIteration = Number(process.hrtime.bigint() - start) /
  probeIterations;
const iterations = Math.ceil(1e9 / nsPerIteration);

setTimeout(() => {
  // uv_library_shutdown() waits for this job, so the main thread stays out of
  // JS and cannot serve the GC that the stress allocation task asks for.
  pbkdf2('a', 'b', iterations, 64, 'sha512', () => {});
  // Use up the old generation budget, so that the next background allocation
  // fails and parks that task in CollectionBarrier.
  for (let i = 0; i < 40; i++) new Array(1e5).fill(i);
  process.exit(0);
}, 300);
