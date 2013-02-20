// Call fs.readFile over and over again really fast.
// Then see how many times it got called.
// Yes, this is a silly benchmark.  Most benchmarks are silly.

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
  var data = new Buffer(len);
  data.fill('x');
  fs.writeFileSync(filename, data);
  data = null;

  var reads = 0;
  bench.start();
  setTimeout(function() {
    bench.end(reads);
    try { fs.unlinkSync(filename); } catch (e) {}
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
    read();
  }

  var cur = +conf.concurrent;
  while (cur--) read();
}
