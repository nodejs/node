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
const assert = require('assert');

const bench = common.createBenchmark(main, {
  dur: [5],
  len: [1024, 16 * 1024 * 1024],
  concurrent: [1, 10]
});

function main(conf) {
  const len = +conf.len;
  try { fs.unlinkSync(filename); } catch {}
  var data = Buffer.alloc(len, 'x');
  fs.writeFileSync(filename, data);
  data = null;

  var zipData = Buffer.alloc(1024, 'a');

  var reads = 0;
  var zips = 0;
  var benchEnded = false;
  bench.start();
  setTimeout(() => {
    const totalOps = reads + zips;
    benchEnded = true;
    bench.end(totalOps);
    try { fs.unlinkSync(filename); } catch {}
  }, +conf.dur * 1000);

  function read() {
    fs.readFile(filename, afterRead);
  }

  function afterRead(er, data) {
    if (er) {
      if (er.code === 'ENOENT') {
        // Only OK if unlinked by the timer from main.
        assert.ok(benchEnded);
        return;
      }
      throw er;
    }

    if (data.length !== len)
      throw new Error('wrong number of bytes returned');

    reads++;
    if (!benchEnded)
      read();
  }

  function zip() {
    zlib.deflate(zipData, afterZip);
  }

  function afterZip(er, data) {
    if (er)
      throw er;

    zips++;
    if (!benchEnded)
      zip();
  }

  // Start reads
  var cur = +conf.concurrent;
  while (cur--) read();

  // Start a competing zip
  zip();
}
