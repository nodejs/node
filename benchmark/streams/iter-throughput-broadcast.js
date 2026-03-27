// Throughput benchmark: fan-out data to N consumers simultaneously.
// Classic streams use PassThrough + pipe, Web Streams use tee() chains,
// stream/iter uses broadcast() with push() consumers.
'use strict';

const common = require('../common.js');
const { PassThrough, Writable } = require('stream');

const bench = common.createBenchmark(main, {
  api: ['classic', 'webstream', 'iter'],
  consumers: [1, 2, 4],
  datasize: [1024 * 1024, 16 * 1024 * 1024],
  n: [5],
}, {
  flags: ['--experimental-stream-iter'],
});

const CHUNK_SIZE = 64 * 1024;

function main({ api, consumers, datasize, n }) {
  const chunk = Buffer.alloc(CHUNK_SIZE, 'abcdefghij');
  const totalOps = (datasize * n) / (1024 * 1024);

  switch (api) {
    case 'classic':
      return benchClassic(chunk, consumers, datasize, n, totalOps);
    case 'webstream':
      return benchWebStream(chunk, consumers, datasize, n, totalOps);
    case 'iter':
      return benchIter(chunk, consumers, datasize, n, totalOps);
  }
}

function benchClassic(chunk, numConsumers, datasize, n, totalOps) {
  function run(cb) {
    const source = new PassThrough();
    const sinks = [];
    let finished = 0;

    for (let c = 0; c < numConsumers; c++) {
      const w = new Writable({ write(data, enc, cb) { cb(); } });
      source.pipe(w);
      w.on('finish', () => { if (++finished === numConsumers) cb(); });
      sinks.push(w);
    }

    let remaining = datasize;
    function write() {
      let ok = true;
      while (ok && remaining > 0) {
        const size = Math.min(remaining, chunk.length);
        remaining -= size;
        const buf = size === chunk.length ? chunk : chunk.subarray(0, size);
        ok = source.write(buf);
      }
      if (remaining > 0) {
        source.once('drain', write);
      } else {
        source.end();
      }
    }
    write();
  }

  let i = 0;
  bench.start();
  (function next() {
    if (i++ >= n) return bench.end(totalOps);
    run(next);
  })();
}

function benchWebStream(chunk, numConsumers, datasize, n, totalOps) {
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

    // Chain tee() calls to get numConsumers branches.
    // tee() gives 2; for 4 we tee twice at each level.
    const branches = [];
    if (numConsumers === 1) {
      branches.push(rs);
    } else {
      const pending = [rs];
      while (branches.length + pending.length < numConsumers) {
        const stream = pending.shift();
        const [a, b] = stream.tee();
        pending.push(a, b);
      }
      branches.push(...pending);
    }

    const ws = () => new WritableStream({ write() {} });
    await Promise.all(branches.map((b) => b.pipeTo(ws())));
  }

  (async () => {
    bench.start();
    for (let i = 0; i < n; i++) await run();
    bench.end(totalOps);
  })();
}

function benchIter(chunk, numConsumers, datasize, n, totalOps) {
  const { broadcast } = require('stream/iter');

  // No-op consumer: drain all batches without collecting
  async function drain(source) {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of source) { /* drain */ }
  }

  async function run() {
    const { writer, broadcast: bc } = broadcast();
    const consumers = [];
    for (let c = 0; c < numConsumers; c++) {
      consumers.push(drain(bc.push()));
    }

    let remaining = datasize;
    while (remaining > 0) {
      const size = Math.min(remaining, chunk.length);
      remaining -= size;
      const buf = size === chunk.length ? chunk : chunk.subarray(0, size);
      writer.writeSync(buf);
    }
    writer.endSync();

    await Promise.all(consumers);
  }

  (async () => {
    bench.start();
    for (let i = 0; i < n; i++) await run();
    bench.end(totalOps);
  })();
}
