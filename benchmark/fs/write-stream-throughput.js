// test the throughput of the fs.WriteStream class.
'use strict';

const path = require('path');
const common = require('../common.js');
const filename = path.resolve(process.env.NODE_TMPDIR || __dirname,
                              `.removeme-benchmark-garbage-${process.pid}`);
const fs = require('fs');

const bench = common.createBenchmark(main, {
  dur: [5],
  encodingType: ['buf', 'asc', 'utf'],
  size: [2, 1024, 65535, 1024 * 1024]
});

function main({ dur, encodingType, size }) {
  var encoding;

  var chunk;
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

  try { fs.unlinkSync(filename); } catch {}

  var started = false;
  var ended = false;

  var f = fs.createWriteStream(filename);
  f.on('drain', write);
  f.on('open', write);
  f.on('close', done);
  f.on('finish', () => {
    ended = true;
    const written = fs.statSync(filename).size / 1024;
    try { fs.unlinkSync(filename); } catch {}
    bench.end(written / 1024);
  });


  function write() {
    if (!started) {
      started = true;
      setTimeout(() => {
        f.end();
      }, dur * 1000);
      bench.start();
    }

    while (false !== f.write(chunk, encoding));
  }

  function done() {
    if (!ended)
      f.emit('finish');
  }
}
