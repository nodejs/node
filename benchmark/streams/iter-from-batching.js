// Measures batching behavior for stream/iter from() and fromSync()
// with plain synchronous Uint8Array iterables.
'use strict';

const common = require('../common.js');
const { closeSync, openSync, writeSync, writevSync } = require('fs');
const { devNull } = require('os');

const bench = common.createBenchmark(main, {
  method: ['from-first-batch', 'from-sync-writev'],
  chunks: [256, 4096, 16384],
  chunkSize: [16],
  n: [100, 1000],
}, {
  flags: ['--experimental-stream-iter'],
  combinationFilter({ method, chunks, n }) {
    if (n === 1) {
      return true;
    }
    if (method === 'from-first-batch') {
      return n === 1000;
    }
    return n === 100 && chunks !== 16384;
  },
  test: {
    chunks: 256,
    chunkSize: 16,
    n: 1,
  },
});

function main({ method, chunks, chunkSize, n }) {
  switch (method) {
    case 'from-first-batch':
      return benchFromFirstBatch(chunks, chunkSize, n);
    case 'from-sync-writev':
      return benchFromSyncWritev(chunks, chunkSize, n);
  }
}

function* source(chunks, chunk) {
  for (let i = 0; i < chunks; i++) {
    yield chunk;
  }
}

function benchFromFirstBatch(chunks, chunkSize, n) {
  const { from } = require('stream/iter');
  const chunk = new Uint8Array(chunkSize);
  let seen = 0;

  (async () => {
    bench.start();
    for (let i = 0; i < n; i++) {
      const iterator = from(source(chunks, chunk))[Symbol.asyncIterator]();
      const { value, done } = await iterator.next();
      if (done || value.length === 0) {
        throw new Error('expected a batch');
      }
      seen += value.length;
    }
    bench.end(n);
    if (seen === 0) {
      throw new Error('expected chunks');
    }
  })();
}

function benchFromSyncWritev(chunks, chunkSize, n) {
  const { pipeToSync } = require('stream/iter');
  const chunk = new Uint8Array(chunkSize);
  const expected = chunks * chunkSize * n;
  let seen = 0;
  let total = 0;
  const fd = openSync(devNull, 'w');
  const writer = {
    writeSync(chunk) {
      writeSync(fd, chunk);
      seen++;
    },
    writevSync(batch) {
      writevSync(fd, batch);
      seen += batch.length;
    },
  };

  try {
    bench.start();
    for (let i = 0; i < n; i++) {
      total += pipeToSync(source(chunks, chunk), writer);
    }
    bench.end(chunks * n);
  } finally {
    closeSync(fd);
  }

  if (total !== expected || seen !== chunks * n) {
    throw new Error('unexpected chunk count');
  }
}
