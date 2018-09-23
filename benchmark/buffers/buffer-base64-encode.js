var common = require('../common.js');

var bench = common.createBenchmark(main, {});

function main(conf) {
  var N = 64 * 1024 * 1024;
  var b = Buffer(N);
  var s = '';
  for (var i = 0; i < 256; ++i) s += String.fromCharCode(i);
  for (var i = 0; i < N; i += 256) b.write(s, i, 256, 'ascii');
  bench.start();
  for (var i = 0; i < 32; ++i) b.toString('base64');
  bench.end(64);
}
