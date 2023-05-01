'use strict';
const common = require('../common.js');
const { Blob } = require('buffer');
const { randomUUID } = require('node:crypto');

const bench = common.createBenchmark(main, {
  n: [1e6],
  type: ['valid', 'invalid'],
});

function generateDataset(n, type) {
  const dataset = [];
  for (let i = 0; i < n; i += 1) {
    switch (type) {
      case 'valid': {
        dataset.push(URL.createObjectURL(new Blob(['hello'])));
      }
        break;
      case 'invalid': {
        dataset.push(`blob:nodedata:${randomUUID()}`);
      }
        break;
      default:
        continue;
    }
  }
  return dataset;
}

function main({ n, type }) {
  const dataset = generateDataset(n, type);
  bench.start();
  for (let i = 0; i < n; i += 1) {
    URL.revokeObjectURL(dataset[i]);
  }
  bench.end(n);
}
