'use strict';
const common = require('../common.js');
const { addAbortListener } = require('events');

const bench = common.createBenchmark(main, {
  n: [1e6],
  kind: ['addEventListener', 'addEventListener-once', 'addAbortListener'],
});

function main({ n, kind }) {
  const { signal } = new AbortController();
  const noop = () => {};

  switch (kind) {
    case 'addEventListener':
      bench.start();
      for (let i = 0; i < n; i++) {
        signal.addEventListener('abort', noop);
        signal.removeEventListener('abort', noop);
      }
      bench.end(n);
      break;
    case 'addEventListener-once': {
      const options = { once: true };
      bench.start();
      for (let i = 0; i < n; i++) {
        signal.addEventListener('abort', noop, options);
        signal.removeEventListener('abort', noop);
      }
      bench.end(n);
      break;
    }
    case 'addAbortListener':
      bench.start();
      for (let i = 0; i < n; i++) {
        addAbortListener(signal, noop)[Symbol.dispose]();
      }
      bench.end(n);
      break;
    default:
      throw new Error('Invalid kind');
  }
}
