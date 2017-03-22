'use strict';

const path = require('path');
const common = require('../common.js');
const filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  dur: [5],
  file: [''],
  type: ['buf', 'asc', 'utf'],
  size: [2, 1024, 65535, 1024 * 1024]
});

function main(conf) {
  const dur = +conf.dur * 1000;
  const type = conf.type;
  const size = +conf.size;
  const file = (conf.file.length === 0 ? filename : conf.file);
  var encoding;

  var chunk;
  switch (type) {
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
      throw new Error('invalid type');
  }

  try { fs.unlinkSync(file); } catch (e) {}

  var started = false;
  var ending = false;
  var ended = false;
  var written = 0;

  setTimeout(function() {
    ending = true;
    f.end();
  }, dur);

  var f = fs.createWriteStream(file);
  f.on('drain', write);
  f.on('open', write);
  f.on('close', done);
  f.on('finish', function() {
    bench.end(written / 1024);
    ended = true;
    try { fs.unlinkSync(file); } catch (e) {}
  });

  function wroteChunk() {
    written += size;
  }

  function write() {
    // don't try to write after we end, even if a 'drain' event comes.
    // v0.8 streams are so sloppy!
    if (ending)
      return;

    if (!started) {
      started = true;
      bench.start();
    }

    while (f.write(chunk, encoding, wroteChunk) !== false);
  }

  function done() {
    if (!ended)
      f.emit('finish');
  }
}
