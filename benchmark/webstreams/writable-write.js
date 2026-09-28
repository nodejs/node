'use strict';
const common = require('../common.js');
const assert = require('node:assert');
const { WritableStream } = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [1e5],
  type: ['await', 'queued'],
});

async function main({ n, type }) {
  let count = 0;
  const ws = new WritableStream({
    write() {
      count++;
    },
  }, { highWaterMark: type === 'queued' ? n : 1 });
  const writer = ws.getWriter();
  bench.start();
  if (type === 'await') {
    for (let i = 0; i < n; i++)
      await writer.write('a');
  } else {
    for (let i = 1; i < n; i++)
      writer.write('a');
    await writer.write('a');
  }
  bench.end(n);
  await writer.close();
  assert.strictEqual(count, n);
}
