'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  type: ['string', 'object', 'arraybuffer'],
  n: [1e4],
});

function main({ n, type }) {
  const data = [];

  switch (type) {
    case 'string':
      for (let i = 0; i < n; ++i) {
        data.push(new Date().toISOString());
      }
      break;
    case 'object':
      for (let i = 0; i < n; ++i) {
        data.push({ ...process.config });
      }
      break;
    case 'arraybuffer':
      for (let i = 0; i < n; ++i) {
        data.push(new ArrayBuffer(10));
      }
      break;
    default:
      throw new Error('Unsupported payload type');
  }

  const run = type === 'arraybuffer' ? (i) => {
    data[i] = structuredClone(data[i], { transfer: [ data[i] ] });
  } : (i) => {
    data[i] = structuredClone(data[i]);
  };

  bench.start();
  for (let i = 0; i < n; ++i) {
    run(i);
  }
  bench.end(n);
  assert.strictEqual(data.length, n);
}
