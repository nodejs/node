'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['for', 'for-of', 'for-in', 'forEach'],
  count: [5, 10, 20, 100],
  millions: [5]
});

function useFor(n, items, count) {
  var i, j;
  bench.start();
  for (i = 0; i < n; i++) {
    for (j = 0; j < count; j++) {
      /* eslint-disable no-unused-vars */
      const item = items[j];
      /* esline-enable no-unused-vars */
    }
  }
  bench.end(n / 1e6);
}

function useForOf(n, items) {
  var i, item;
  bench.start();
  for (i = 0; i < n; i++) {
    for (item of items) {}
  }
  bench.end(n / 1e6);
}

function useForIn(n, items) {
  var i, j, item;
  bench.start();
  for (i = 0; i < n; i++) {
    for (j in items) {
      /* eslint-disable no-unused-vars */
      item = items[j];
      /* esline-enable no-unused-vars */
    }
  }
  bench.end(n / 1e6);
}

function useForEach(n, items) {
  var i;
  bench.start();
  for (i = 0; i < n; i++) {
    items.forEach((item) => {});
  }
  bench.end(n / 1e6);
}

function main({ millions, count, method }) {
  const n = millions * 1e6;
  const items = new Array(count);
  var i;
  var fn;
  for (i = 0; i < count; i++)
    items[i] = i;

  switch (method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
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
      throw new Error('Unexpected method');
  }
  fn(n, items, count);
}
