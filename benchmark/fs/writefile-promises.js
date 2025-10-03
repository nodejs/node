// Call fs.promises.writeFile over and over again really fast.
// Then see how many times it got called.
// Yes, this is a silly benchmark.  Most benchmarks are silly.
'use strict';

const common = require('../common.js');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');

tmpdir.refresh();
const filename = tmpdir.resolve(`.removeme-benchmark-garbage-${process.pid}`);
let filesWritten = 0;
const bench = common.createBenchmark(main, {
  duration: [5],
  encodingType: ['buf', 'asc', 'utf'],
  size: [2, 1024, 65535, 1024 * 1024],
  concurrent: [1, 10],
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
  let waitConcurrent = 0;

  let startedAt = Date.now();
  let endAt = startedAt + duration * 1000;

  // fs warmup
  for (let i = 0; i < concurrent; i++) write();

  writes = 0;
  waitConcurrent = 0;

  startedAt = Date.now();
  endAt = startedAt + duration * 1000;

  bench.start();

  function stop() {
    bench.end(writes);

    for (let i = 0; i < filesWritten; i++) {
      try {
        fs.unlinkSync(`${filename}-${i}`);
      } catch {
        // Continue regardless of error.
      }
    }

    process.exit(0);
  }

  function write() {
    fs.promises
      .writeFile(`${filename}-${filesWritten++}`, chunk, encoding)
      .then(() => afterWrite())
      .catch((err) => afterWrite(err));
  }

  function afterWrite(er) {
    if (er) {
      throw er;
    }

    writes++;
    const benchEnded = Date.now() >= endAt;

    if (benchEnded && ++waitConcurrent === concurrent) {
      stop();
    } else if (!benchEnded) {
      write();
    }
  }

  for (let i = 0; i < concurrent; i++) write();
}
