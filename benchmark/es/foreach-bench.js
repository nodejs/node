'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['for', 'for-of', 'for-in', 'forEach'],
  count: [5, 10, 20, 100],
  n: [5e6]
});

function useFor(n, items, count) {
  bench.start();
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < count; j++) {
      // eslint-disable-next-line no-unused-vars
      const item = items[j];
    }
  }
  bench.end(n);
}

function useForOf(n, items) {
  bench.start();
  for (let i = 0; i < n; i++) {
    // eslint-disable-next-line no-unused-vars
    for (const item of items) {}
  }
  bench.end(n);
}

function useForIn(n, items) {
  bench.start();
  for (let i = 0; i < n; i++) {
    for (const j in items) {
      // eslint-disable-next-line no-unused-vars
      const item = items[j];
    }
  }
  bench.end(n);
}

function useForEach(n, items) {
  bench.start();
  for (let i = 0; i < n; i++) {
    items.forEach((item) => {});
  }
  bench.end(n);
}

function main({ n, count, method }) {
  const items = new Array(count);
  let fn;
  for (let i = 0; i < count; i++)
    items[i] = i;

  switch (method) {
    case 'for':
      fn = useFor;
      break;
    case 'for-of':
      fn = useForOf;
      break;
    case 'for-in':
      fn = useForIn;
      break;
    case 'forEach':
      fn = useForEach;
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
  fn(n, items, count);
}
