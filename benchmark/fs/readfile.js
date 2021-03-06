// Call fs.readFile over and over again really fast.
// Then see how many times it got called.
// Yes, this is a silly benchmark.  Most benchmarks are silly.
'use strict';

const path = require('path');
const common = require('../common.js');
const fs = require('fs');
const assert = require('assert');

const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();
const filename = path.resolve(tmpdir.path,
                              `.removeme-benchmark-garbage-${process.pid}`);

const bench = common.createBenchmark(main, {
  duration: [5],
  len: [1024, 16 * 1024 * 1024],
  concurrent: [1, 10]
});

function main({ len, duration, concurrent }) {
  try { fs.unlinkSync(filename); } catch {}
  let data = Buffer.alloc(len, 'x');
  fs.writeFileSync(filename, data);
  data = null;

  let reads = 0;
  let benchEnded = false;
  bench.start();
  setTimeout(() => {
    benchEnded = true;
    bench.end(reads);
    try { fs.unlinkSync(filename); } catch {}
    process.exit(0);
  }, duration * 1000);

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

  while (concurrent--) read();
}
