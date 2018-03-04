'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['for', 'for-of', 'for-in', 'forEach'],
  count: [5, 10, 20, 100],
  millions: [5],
});

function useFor(millions, items, count) {
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    for (var j = 0; j < count; j++) {
      /* eslint-disable no-unused-vars */
      const item = items[j];
      /* esline-enable no-unused-vars */
    }
  }
  bench.end(millions);
}

function useForOf(millions, items) {
  var item;
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    for (item of items) {}
  }
  bench.end(millions);
}

function useForIn(millions, items) {
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    for (var j in items) {
      /* eslint-disable no-unused-vars */
      const item = items[j];
      /* esline-enable no-unused-vars */
    }
  }
  bench.end(millions);
}

function useForEach(millions, items) {
  bench.start();
  for (var i = 0; i < millions * 1e6; i++) {
    items.forEach((item) => {});
  }
  bench.end(millions);
}

function main({ millions, count, method }) {
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
  fn(millions, items, count);
}
