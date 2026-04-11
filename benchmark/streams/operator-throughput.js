'use strict';

const assert = require('assert');
const common = require('../common');
const { Readable } = require('stream');

const bench = common.createBenchmark(main, {
  n: [2e5],
  operation: [
    'reduce',
    'reduce-async',
    'map-sync-1',
    'map-sync-8',
    'map-async-1',
    'map-async-8',
    'find-sync-1',
    'find-sync-8',
    'find-async-1',
    'find-async-8',
    'filter',
    'forEach',
    'toArray',
  ],
  source: ['readable', 'iterator', 'array'],
});

function createSource(type, n) {
  if (type === 'array') {
    return Readable.from(Array.from({ length: n }, (_, i) => i));
  }
  let i = 0;

  if (type === 'readable') {
    return new Readable({
      objectMode: true,
      read() {
        this.push(i === n ? null : i++);
      },
    });
  }

  return Readable.from((function* generate() {
    while (i < n) {
      yield i++;
    }
  })());
}

async function main({ n, operation, source }) {
  const readable = createSource(source, n);
  let result;

  bench.start();

  switch (operation) {
    case 'reduce':
      result = await readable.reduce((sum, value) => sum + value, 0);
      break;
    case 'reduce-async':
      result = await readable.reduce(async (sum, value) => sum + value, 0);
      break;
    case 'map-sync-1':
      result = await readable
        .map((value) => value + 1, { concurrency: 1 })
        .reduce((sum, value) => sum + value, 0);
      break;
    case 'map-sync-8':
      result = await readable
        .map((value) => value + 1, { concurrency: 8 })
        .reduce((sum, value) => sum + value, 0);
      break;
    case 'map-async-1':
      result = await readable
        .map(async (value) => value + 1, { concurrency: 1 })
        .reduce((sum, value) => sum + value, 0);
      break;
    case 'map-async-8':
      result = await readable
        .map(async (value) => value + 1, { concurrency: 8 })
        .reduce((sum, value) => sum + value, 0);
      break;
    case 'find-sync-1':
      result = await readable.find((value) => value === n - 1, { concurrency: 1 });
      break;
    case 'find-sync-8':
      result = await readable.find((value) => value === n - 1, { concurrency: 8 });
      break;
    case 'find-async-1':
      result = await readable.find(async (value) => value === n - 1, { concurrency: 1 });
      break;
    case 'find-async-8':
      result = await readable.find(async (value) => value === n - 1, { concurrency: 8 });
      break;
    case 'filter':
      result = await readable
        .filter((value) => (value & 1) === 0)
        .reduce((sum, value) => sum + value, 0);
      break;
    case 'forEach':
      result = 0;
      await readable.forEach((value) => { result += value; });
      break;
    case 'toArray':
      result = await readable.toArray();
      break;
    default:
      throw new Error(`Unknown operation: ${operation}`);
  }

  bench.end(n);

  const sum = n * (n - 1) / 2;
  switch (operation) {
    case 'map-sync-1':
    case 'map-sync-8':
    case 'map-async-1':
    case 'map-async-8':
      assert.strictEqual(result, sum + n);
      break;
    case 'find-sync-1':
    case 'find-sync-8':
    case 'find-async-1':
    case 'find-async-8':
      assert.strictEqual(result, n - 1);
      break;
    case 'filter': {
      const evenCount = Math.ceil(n / 2);
      assert.strictEqual(result, evenCount * (evenCount - 1));
      break;
    }
    case 'toArray':
      assert.strictEqual(result.length, n);
      assert.strictEqual(result[n - 1], n - 1);
      break;
    default:
      assert.strictEqual(result, sum);
  }
}
