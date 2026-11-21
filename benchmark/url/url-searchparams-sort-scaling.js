'use strict';

const common = require('../common.js');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e4],
  size: [49, 50, 65, 256, 1024],
  order: ['sorted', 'reversed', 'shuffled', 'duplicates'],
});

function main({ n, size, order }) {
  const entries = Array.from({ length: size }, (_, index) => [
    `key-${String(order === 'duplicates' ? index % 7 : index).padStart(8, '0')}`,
    String(index),
  ]);
  if (order === 'reversed') {
    entries.reverse();
  } else if (order === 'shuffled') {
    let seed = 12345;
    for (let index = entries.length - 1; index > 0; index--) {
      seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
      const other = seed % (index + 1);
      [entries[index], entries[other]] = [entries[other], entries[index]];
    }
  }
  const base = new URLSearchParams(entries);
  const expected = entries.slice().sort(([first], [second]) =>
    (first < second ? -1 : first > second ? 1 : 0));

  for (let index = 0; index < 1000; index++) {
    new URLSearchParams(base).sort();
  }
  let result;
  bench.start();
  for (let index = 0; index < n; index++) {
    result = new URLSearchParams(base);
    result.sort();
  }
  bench.end(n);
  assert.deepStrictEqual([...result], expected);
  assert.deepStrictEqual([...base], entries);
}
