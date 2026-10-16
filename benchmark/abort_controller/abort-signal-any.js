'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [2e5],
  kind: ['create', 'listen', 'abort'],
});

// Combines a per-iteration signal with a long-lived one, like a request
// signal combined with a server wide one. The work is split into batches
// that run in separate macrotasks because WeakRef targets are kept alive
// until the end of the current job, as they would be between requests.
function main({ n, kind }) {
  const reason = new Error('reason');
  const noop = () => {};
  const longLived = new AbortController().signal;
  const batch = 1000;

  function run(count) {
    for (let i = 0; i < count; i++) {
      const ac = new AbortController();
      const signal = AbortSignal.any([ac.signal, longLived]);
      if (kind === 'create')
        continue;
      signal.addEventListener('abort', noop, { once: true });
      if (kind === 'listen')
        signal.removeEventListener('abort', noop);
      else
        ac.abort(reason);
    }
  }

  let left = n;
  function step() {
    const count = Math.min(batch, left);
    run(count);
    left -= count;
    if (left > 0)
      setImmediate(step);
    else
      bench.end(n);
  }

  bench.start();
  step();
}
