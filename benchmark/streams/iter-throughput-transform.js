// Throughput benchmark: data flow through a single stateless transform.
// Uses buffer copy (allocate + memcpy) so pipeline overhead is measurable.
'use strict';

const common = require('../common.js');
const { Readable, Transform, Writable, pipeline } = require('stream');

const bench = common.createBenchmark(main, {
  api: ['classic', 'webstream', 'iter', 'iter-sync'],
  datasize: [1024 * 1024, 16 * 1024 * 1024, 64 * 1024 * 1024],
  n: [5],
}, {
  flags: ['--experimental-stream-iter'],
});

const CHUNK_SIZE = 64 * 1024;

// Buffer copy transform: allocate + memcpy. Cheap enough that pipeline
// overhead is a measurable fraction of total time, but non-trivial (new
// buffer per chunk, so it's a real transform that produces new data).
function copyBuf(buf) {
  return Buffer.copyBytesFrom(buf);
}

function main({ api, datasize, n }) {
  const chunk = Buffer.alloc(CHUNK_SIZE, 'abcdefghij');
  const totalOps = (datasize * n) / (1024 * 1024);

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
  function run(cb) {
    let remaining = datasize;
    const r = new Readable({
      read() {
        if (remaining <= 0) { this.push(null); return; }
        const size = Math.min(remaining, chunk.length);
        remaining -= size;
        this.push(size === chunk.length ? chunk : chunk.subarray(0, size));
      },
    });
    const t = new Transform({
      transform(data, enc, cb) {
        cb(null, copyBuf(data));
      },
    });
    const w = new Writable({ write(data, enc, cb) { cb(); } });
    pipeline(r, t, w, cb);
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
        if (remaining <= 0) { controller.close(); return; }
        const size = Math.min(remaining, chunk.length);
        remaining -= size;
        controller.enqueue(
          size === chunk.length ? chunk : chunk.subarray(0, size));
      },
    });
    const ts = new TransformStream({
      transform(c, controller) {
        controller.enqueue(copyBuf(c));
      },
    });
    const ws = new WritableStream({ write() {} });
    await rs.pipeThrough(ts).pipeTo(ws);
  }

  (async () => {
    bench.start();
    for (let i = 0; i < n; i++) await run();
    bench.end(totalOps);
  })();
}

function benchIter(chunk, datasize, n, totalOps) {
  const { pipeTo } = require('stream/iter');

  const upper = (chunks) => {
    if (chunks === null) return null;
    return chunks.map((c) => copyBuf(c));
  };

  async function run() {
    let remaining = datasize;
    async function* source() {
      while (remaining > 0) {
        const size = Math.min(remaining, chunk.length);
        remaining -= size;
        yield [size === chunk.length ? chunk : chunk.subarray(0, size)];
      }
    }
    await pipeTo(source(), upper,
                 { write() {}, writeSync() { return true; } });
  }

  (async () => {
    bench.start();
    for (let i = 0; i < n; i++) await run();
    bench.end(totalOps);
  })();
}

function benchIterSync(chunk, datasize, n, totalOps) {
  const { pipeToSync } = require('stream/iter');

  const upper = (chunks) => {
    if (chunks === null) return null;
    return chunks.map((c) => copyBuf(c));
  };

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
    pipeToSync(source(), upper, { writeSync() {} });
  }
  bench.end(totalOps);
}
