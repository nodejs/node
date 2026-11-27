'use strict';

const common = require('../common.js');
const { Readable } = require('node:stream');
const consumers = require('node:stream/consumers');

const bench = common.createBenchmark(main, {
  method: ['arrayBuffer', 'buffer', 'bytes', 'blob'],
  type: ['buffer', 'string'],
  chunks: [4, 32],
  n: [2e3],
});

async function main({ n, method, type, chunks }) {
  const consume = consumers[method];
  const chunk = type === 'buffer' ?
    Buffer.alloc(1024, 65) :
    'a'.repeat(1024);
  const data = Array.from({ length: chunks }, () => chunk);

  // Warmup
  for (let i = 0; i < 100; i++) {
    await consume(Readable.from(data));
  }

  let total = 0;
  bench.start();
  for (let i = 0; i < n; i++) {
    const res = await consume(Readable.from(data));
    total += res.byteLength ?? res.size;
  }
  bench.end(n);

  if (total === 0) {
    throw new Error('Unexpected empty result');
  }
}
