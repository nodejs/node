// Call fs.readFile with permission system enabled
// over and over again really fast.
// Then see how many times it got called.
'use strict';

const common = require('../common.js');
const fs = require('fs');

const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();
const filename = tmpdir.resolve(`.removeme-benchmark-garbage-${process.pid}`);

const bench = common.createBenchmark(main, {
  duration: [5],
  encoding: ['', 'utf-8'],
  len: [1024, 16 * 1024 * 1024],
  concurrent: [1, 10],
}, {
  flags: [
    '--permission',
    '--allow-fs-read=*',
    '--allow-fs-write=*',
    '--allow-child-process',
  ],
});

function main({ len, duration, concurrent, encoding }) {
  try {
    fs.unlinkSync(filename);
  } catch {
    // Continue regardless of error.
  }
  let data = Buffer.alloc(len, 'x');
  fs.writeFileSync(filename, data);
  data = null;

  let reads = 0;
  let waitConcurrent = 0;

  const startedAt = Date.now();
  const endAt = startedAt + (duration * 1000);

  bench.start();

  function stop() {
    bench.end(reads);

    try {
      fs.unlinkSync(filename);
    } catch {
      // Continue regardless of error.
    }

    process.exit(0);
  }

  function read() {
    fs.readFile(filename, encoding, afterRead);
  }

  function afterRead(er, data) {
    if (er) {
      throw er;
    }

    if (data.length !== len)
      throw new Error('wrong number of bytes returned');

    reads++;
    const benchEnded = Date.now() >= endAt;

    if (benchEnded && (++waitConcurrent) === concurrent) {
      stop();
    } else if (!benchEnded) {
      read();
    }
  }

  for (let i = 0; i < concurrent; i++) read();
}
