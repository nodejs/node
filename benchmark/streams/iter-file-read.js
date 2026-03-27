// File reading throughput benchmark.
// Reads a real file through the three stream APIs.
'use strict';

const common = require('../common.js');
const fs = require('fs');
const { Writable, pipeline } = require('stream');
const tmpdir = require('../../test/common/tmpdir');

tmpdir.refresh();
const filename = tmpdir.resolve(`.removeme-bench-file-read-${process.pid}`);

const bench = common.createBenchmark(main, {
  api: ['classic', 'webstream', 'iter'],
  filesize: [1024 * 1024, 16 * 1024 * 1024, 64 * 1024 * 1024],
  n: [5],
}, {
  flags: ['--experimental-stream-iter'],
});

function main({ api, filesize, n }) {
  // Create fixture file
  const chunk = Buffer.alloc(Math.min(filesize, 64 * 1024), 'abcdefghij');
  const fd = fs.openSync(filename, 'w');
  let remaining = filesize;
  while (remaining > 0) {
    const size = Math.min(remaining, chunk.length);
    fs.writeSync(fd, chunk, 0, size);
    remaining -= size;
  }
  fs.closeSync(fd);

  const totalOps = (filesize * n) / (1024 * 1024);

  switch (api) {
    case 'classic':
      return benchClassic(filesize, n, totalOps);
    case 'webstream':
      return benchWebStream(filesize, n, totalOps);
    case 'iter':
      return benchIter(filesize, n, totalOps);
  }
}

function benchClassic(filesize, n, totalOps) {
  function run(cb) {
    const r = fs.createReadStream(filename);
    const w = new Writable({ write(data, enc, cb) { cb(); } });
    pipeline(r, w, cb);
  }

  // Warmup
  run(() => {
    let i = 0;
    bench.start();
    (function next() {
      if (i++ >= n) {
        fs.unlinkSync(filename);
        return bench.end(totalOps);
      }
      run(next);
    })();
  });
}

function benchWebStream(filesize, n, totalOps) {
  const fsp = require('fs/promises');

  async function run() {
    const fh = await fsp.open(filename, 'r');
    const rs = fh.readableWebStream();
    const ws = new WritableStream({ write() {} });
    await rs.pipeTo(ws);
    await fh.close();
  }

  (async () => {
    // Warmup
    await run();

    bench.start();
    for (let i = 0; i < n; i++) await run();
    fs.unlinkSync(filename);
    bench.end(totalOps);
  })();
}

function benchIter(filesize, n, totalOps) {
  const fsp = require('fs/promises');
  const { pipeTo } = require('stream/iter');

  async function run() {
    const fh = await fsp.open(filename, 'r');
    await pipeTo(fh.pull(), { write() {} });
    await fh.close();
  }

  (async () => {
    // Warmup
    await run();

    bench.start();
    for (let i = 0; i < n; i++) await run();
    fs.unlinkSync(filename);
    bench.end(totalOps);
  })();
}
