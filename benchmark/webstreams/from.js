'use strict';
const common = require('../common.js');
const {
  ReadableStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [1e6],
  kind: ['sync', 'async'],
});

async function main({ n, kind }) {
  function* syncGen() {
    for (let i = 0; i < n; i++) yield i;
  }

  async function* asyncGen() {
    for (let i = 0; i < n; i++) yield i;
  }

  const reader = ReadableStream.from(
    kind === 'sync' ? syncGen() : asyncGen()).getReader();
  bench.start();
  for (;;) {
    const { done } = await reader.read();
    if (done) break;
  }
  bench.end(n);
}
