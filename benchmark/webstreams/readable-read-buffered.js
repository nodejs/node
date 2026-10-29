'use strict';
const common = require('../common.js');
const { ReadableStream } = require('node:stream/web');

// Benchmark for reading from a pre-buffered ReadableStream.
// This measures the fast path optimization where data is already
// queued in the controller, avoiding DefaultReadRequest allocation.

const bench = common.createBenchmark(main, {
  n: [1e5],
  bufferSize: [1, 10, 100, 1000],
});

async function main({ n, bufferSize }) {
  let enqueued = 0;

  const rs = new ReadableStream({
    start(controller) {
      // Pre-fill the buffer
      for (let i = 0; i < bufferSize; i++) {
        controller.enqueue('a');
        enqueued++;
      }
    },
    pull(controller) {
      // Refill buffer when pulled
      const toEnqueue = Math.min(bufferSize, n - enqueued);
      for (let i = 0; i < toEnqueue; i++) {
        controller.enqueue('a');
        enqueued++;
      }
      if (enqueued >= n) {
        controller.close();
      }
    },
  }, {
    // Use buffer size as high water mark to allow pre-buffering
    highWaterMark: bufferSize,
  });

  const reader = rs.getReader();
  let x = null;
  let reads = 0;

  bench.start();
  while (reads < n) {
    const { value, done } = await reader.read();
    if (done) break;
    x = value;
    reads++;
  }
  bench.end(reads);
  console.assert(x);
}
