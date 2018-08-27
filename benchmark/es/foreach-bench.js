'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['for', 'for-of', 'for-in', 'forEach'],
  count: [5, 10, 20, 100],
  n: [5e6]
});

function useFor(n, items, count) {
  bench.start();
  for (var i = 0; i < n; i++) {
    for (var j = 0; j < count; j++) {
      /* eslint-disable no-unused-vars */
      const item = items[j];
      /* esline-enable no-unused-vars */
    }
  }
  bench.end(n);
}

function useForOf(n, items) {
  var item;
  bench.start();
  for (var i = 0; i < n; i++) {
    for (item of items) {}
  }
  bench.end(n);
}

function useForIn(n, items) {
  bench.start();
  for (var i = 0; i < n; i++) {
    for (var j in items) {
      /* eslint-disable no-unused-vars */
      const item = items[j];
      /* esline-enable no-unused-vars */
    }
  }
  bench.end(n);
}

function useForEach(n, items) {
  bench.start();
  for (var i = 0; i < n; i++) {
    items.forEach((item) => {});
  }
  bench.end(n);
}

function main({ n, count, method }) {
  const items = new Array(count);
  var fn;
  for (var i = 0; i < count; i++)
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
      throw new Error(`Unexpected method "${method}"`);
  }
  fn(n, items, count);
}
