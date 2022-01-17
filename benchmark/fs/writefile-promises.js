// Call fs.promises.writeFile over and over again really fast.
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
let filesWritten = 0;
const bench = common.createBenchmark(main, {
  duration: [5],
  encodingType: ['buf', 'asc', 'utf'],
  size: [2, 1024, 65535, 1024 * 1024],
  concurrent: [1, 10]
});

function main({ encodingType, duration, concurrent, size }) {
  let encoding;
  let chunk;
  switch (encodingType) {
    case 'buf':
      chunk = Buffer.alloc(size, 'b');
      break;
    case 'asc':
      chunk = 'a'.repeat(size);
      encoding = 'ascii';
      break;
    case 'utf':
      chunk = 'ü'.repeat(Math.ceil(size / 2));
      encoding = 'utf8';
      break;
    default:
      throw new Error(`invalid encodingType: ${encodingType}`);
  }

  let writes = 0;
  let benchEnded = false;
  bench.start();
  setTimeout(() => {
    benchEnded = true;
    bench.end(writes);
    for (let i = 0; i < filesWritten; i++) {
      try { fs.unlinkSync(`${filename}-${i}`); } catch { }
    }
    process.exit(0);
  }, duration * 1000);

  function write() {
    fs.promises.writeFile(`${filename}-${filesWritten++}`, chunk, encoding)
      .then(() => afterWrite())
      .catch((err) => afterWrite(err));
  }

  function afterWrite(er) {
    if (er) {
      if (er.code === 'ENOENT') {
        // Only OK if unlinked by the timer from main.
        assert.ok(benchEnded);
        return;
      }
      throw er;
    }

    writes++;
    if (!benchEnded)
      write();
  }

  while (concurrent--) write();
}
