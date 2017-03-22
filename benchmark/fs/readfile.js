'use strict';

const path = require('path');
const common = require('../common.js');
const filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  dur: [5],
  len: [1024, 16 * 1024 * 1024],
  concurrent: [1, 10]
});

function main(conf) {
  const len = +conf.len;
  const dur = +conf.dur * 1000;
  var cur = +conf.concurrent;
  var reads = 0;

  try { fs.unlinkSync(filename); } catch (e) {}
  fs.writeFileSync(filename, Buffer.alloc(len, 'x'));

  bench.start();
  setTimeout(function() {
    bench.end(reads);
    try { fs.unlinkSync(filename); } catch (e) {}
    process.exit(0);
  }, dur);

  function read() {
    fs.readFile(filename, afterRead);
  }

  function afterRead(er, data) {
    if (er)
      throw er;

    if (data.length !== len)
      throw new Error('wrong number of bytes returned');

    reads++;
    read();
  }

  while (cur--) read();
}
