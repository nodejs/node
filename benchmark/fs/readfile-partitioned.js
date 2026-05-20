// Submit a mix of short and long jobs to the threadpool.
// Report total job throughput.
// If we partition the long job, overall job throughput goes up significantly.
// However, this comes at the cost of the long job throughput.
//
// Short jobs: small zip jobs.
// Long jobs: fs.readFile on a large file.

'use strict';

const path = require('path');
const common = require('../common.js');
const filename = path.resolve(__dirname,
                              `.removeme-benchmark-garbage-${process.pid}`);
const fs = require('fs');
const zlib = require('zlib');

const bench = common.createBenchmark(main, {
  duration: [5],
  encoding: ['', 'utf-8'],
  len: [1024, 16 * 1024 * 1024],
  concurrent: [1, 10],
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

  const zipData = Buffer.alloc(1024, 'a');

  let waitConcurrent = 0;

  // Plus one because of zip
  const targetConcurrency = concurrent + 1;
  const startedAt = Date.now();
  const endAt = startedAt + (duration * 1000);

  let reads = 0;
  let zips = 0;

  bench.start();

  function stop() {
    const totalOps = reads + zips;
    bench.end(totalOps);

    try {
      fs.unlinkSync(filename);
    } catch {
      // Continue regardless of error.
    }
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

    if (benchEnded && (++waitConcurrent) === targetConcurrency) {
      stop();
    } else if (!benchEnded) {
      read();
    }
  }

  function zip() {
    zlib.deflate(zipData, afterZip);
  }

  function afterZip(er, data) {
    if (er)
      throw er;

    zips++;
    const benchEnded = Date.now() >= endAt;

    if (benchEnded && (++waitConcurrent) === targetConcurrency) {
      stop();
    } else if (!benchEnded) {
      zip();
    }
  }

  // Start reads
  for (let i = 0; i < concurrent; i++) read();

  // Start a competing zip
  zip();
}
