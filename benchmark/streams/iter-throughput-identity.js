// Throughput benchmark: raw data flow from source to consumer, no transforms.
// Compares Node.js classic streams, Web Streams, and stream/iter.
'use strict';

const common = require('../common.js');
const { Readable, Writable, pipeline } = require('stream');

const bench = common.createBenchmark(main, {
  api: ['classic', 'webstream', 'iter', 'iter-sync'],
  datasize: [1024 * 1024, 16 * 1024 * 1024, 64 * 1024 * 1024],
  n: [5],
}, {
  flags: ['--experimental-stream-iter'],
});

const CHUNK_SIZE = 64 * 1024;

function main({ api, datasize, n }) {
  const chunk = Buffer.alloc(CHUNK_SIZE, 'abcdefghij');
  const totalOps = (datasize * n) / (1024 * 1024); // MB

  switch (api) {
    case 'classic':
      return benchClassic(chunk, datasize, n, totalOps);
    case 'webstream':
      return benchWebStream(chunk, datasize, n, totalOps);
    case 'iter':
      return benchIter(chunk, datasize, n, totalOps);
    case 'iter-sync':
      return benchIterSync(chunk, datasize, n, totalOps);
  }
}

function benchClassic(chunk, datasize, n, totalOps) {
  let remaining = 0;

  function run(cb) {
    remaining = datasize;
    const r = new Readable({
      read() {
        if (remaining <= 0) {
          this.push(null);
          return;
        }
        const size = Math.min(remaining, chunk.length);
        remaining -= size;
        this.push(size === chunk.length ? chunk : chunk.subarray(0, size));
      },
    });
    const w = new Writable({
      write(data, enc, cb) { cb(); },
    });
    pipeline(r, w, cb);
  }

  let i = 0;
  bench.start();
  (function next() {
    if (i++ >= n) return bench.end(totalOps);
    run(next);
  })();
}

function benchWebStream(chunk, datasize, n, totalOps) {
  async function run() {
    let remaining = datasize;
    const rs = new ReadableStream({
      pull(controller) {
        if (remaining <= 0) {
          controller.close();
          return;
        }
        const size = Math.min(remaining, chunk.length);
        remaining -= size;
        controller.enqueue(
          size === chunk.length ? chunk : chunk.subarray(0, size));
      },
    });
    const ws = new WritableStream({
      write() {},
    });
    await rs.pipeTo(ws);
  }

  (async () => {
    bench.start();
    for (let i = 0; i < n; i++) await run();
    bench.end(totalOps);
  })();
}

function benchIter(chunk, datasize, n, totalOps) {
  const { pipeTo } = require('stream/iter');

  async function run() {
    let remaining = datasize;
    async function* source() {
      while (remaining > 0) {
        const size = Math.min(remaining, chunk.length);
        remaining -= size;
        yield [size === chunk.length ? chunk : chunk.subarray(0, size)];
      }
    }
    // Drain to no-op sink, matching classic/webstream behavior
    await pipeTo(source(), { write() {}, writeSync() { return true; } });
  }

  (async () => {
    bench.start();
    for (let i = 0; i < n; i++) await run();
    bench.end(totalOps);
  })();
}

function benchIterSync(chunk, datasize, n, totalOps) {
  const { pipeToSync } = require('stream/iter');

  bench.start();
  for (let i = 0; i < n; i++) {
    let remaining = datasize;
    function* source() {
      while (remaining > 0) {
        const size = Math.min(remaining, chunk.length);
        remaining -= size;
        yield [size === chunk.length ? chunk : chunk.subarray(0, size)];
      }
    }
    // Drain to no-op sink, matching other benchmarks
    pipeToSync(source(), { writeSync() {} });
  }
  bench.end(totalOps);
}
