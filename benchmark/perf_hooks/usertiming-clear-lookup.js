'use strict';

const common = require('../common.js');
const { performance } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  operation: ['clearMarks', 'getEntriesByName', 'getEntriesByType', 'markAfterIndex'],
  entries: [10, 100, 1000, 5000],
  n: [2000],
});

function main({ operation, entries, n }) {
  const names = new Array(entries);
  for (let i = 0; i < entries; i++) {
    const name = `mark-${i}`;
    names[i] = name;
    performance.mark(name);
  }

  let total = 0;
  if (operation === 'clearMarks') {
    bench.start();
    for (let i = 0; i < n; i++) {
      const name = names[i % entries];
      performance.clearMarks(name);
      performance.mark(name);
    }
    bench.end(n);
  } else if (operation === 'getEntriesByName') {
    bench.start();
    for (let i = 0; i < n; i++) {
      total += performance.getEntriesByName(names[i % entries]).length;
    }
    bench.end(n);
  } else if (operation === 'getEntriesByType') {
    // Build the index beforehand (for buffers above the indexing threshold)
    // to verify full reads do not regress.
    total += performance.getEntriesByName(names[0]).length;
    total += performance.getEntriesByName(names[0]).length;
    bench.start();
    for (let i = 0; i < n; i++) {
      total += performance.getEntriesByType('mark').length;
    }
    bench.end(n);
  } else {
    // Build the index beforehand (for buffers above the indexing threshold)
    // to verify subsequent mark() calls do not regress.
    total += performance.getEntriesByName(names[0]).length;
    total += performance.getEntriesByName(names[0]).length;
    bench.start();
    for (let i = 0; i < n; i++) {
      performance.mark(names[i % entries]);
    }
    bench.end(n);
  }

  performance.clearMarks();
  return total;
}
