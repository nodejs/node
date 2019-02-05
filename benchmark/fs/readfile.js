// Call fs.readFile over and over again really fast.
// Then see how many times it got called.
// Yes, this is a silly benchmark.  Most benchmarks are silly.
'use strict';

const path = require('path');
const common = require('../common.js');
const filename = path.resolve(process.env.NODE_TMPDIR || __dirname,
                              `.removeme-benchmark-garbage-${process.pid}`);
const fs = require('fs');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  dur: [5],
  len: [1024, 16 * 1024 * 1024],
  concurrent: [1, 10]
});

function main({ len, dur, concurrent }) {
  try { fs.unlinkSync(filename); } catch {}
  var data = Buffer.alloc(len, 'x');
  fs.writeFileSync(filename, data);
  data = null;

  var reads = 0;
  var benchEnded = false;
  bench.start();
  setTimeout(() => {
    benchEnded = true;
    bench.end(reads);
    try { fs.unlinkSync(filename); } catch {}
    process.exit(0);
  }, dur * 1000);

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
