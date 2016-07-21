// Call fs.readFile over and over again really fast.
// Then see how many times it got called.
// Yes, this is a silly benchmark.  Most benchmarks are silly.
'use strict';

var path = require('path');
var common = require('../common.js');
var filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
var fs = require('fs');

var bench = common.createBenchmark(main, {
  dur: [5],
  len: [1024, 16 * 1024 * 1024],
  concurrent: [1, 10]
});

function main(conf) {
  var len = +conf.len;
  try { fs.unlinkSync(filename); } catch (e) {}
  var data = Buffer.alloc(len, 'x');
  fs.writeFileSync(filename, data);
  data = null;

  var reads = 0;
  var bench_ended = false;
  bench.start();
  setTimeout(function() {
    bench_ended = true;
    bench.end(reads);
    try { fs.unlinkSync(filename); } catch (e) {}
    process.exit(0);
  }, +conf.dur * 1000);

  function read() {
    fs.readFile(filename, afterRead);
  }

  function afterRead(er, data) {
    if (er)
      throw er;

    if (data.length !== len)
      throw new Error('wrong number of bytes returned');

    reads++;
    if (!bench_ended)
      read();
  }

  var cur = +conf.concurrent;
  while (cur--) read();
}
