'use strict';

const common = require('../common');

// Oscillates the FixedQueue between N and N+1 segments repeatedly to
// exercise the spare-segment reuse path: push k items to force a new segment,
// then shift k items to empty one segment; repeat.
const bench = common.createBenchmark(main, {
  cycles: [5e3],
}, { flags: ['--expose-internals'] });

function main({ cycles }) {
  const FixedQueue = require('internal/fixed_queue');
  const q = new FixedQueue();
  // Derive the internal segment size (kSize) from the current head.
  const k = q.head.list.length;

  bench.start();
  for (let c = 0; c < cycles; c++) {
    // Grow to N+1 by pushing k items: k-1 fills the current segment,
    // the k-th push creates/uses the next segment.
    for (let i = 0; i < k; i++) q.push(i);
    // Shrink back to N by shifting k items: empties one segment and advances.
    for (let i = 0; i < k; i++) q.shift();
  }
  // Report total operations (push + shift) to normalize throughput.
  bench.end(cycles * 2 * k);
}
