var assert = require('assert');
var common = require('../common.js');

var bench = common.createBenchmark(main, {});

function main(conf) {
  for (var s = 'abcd'; s.length < 32 << 20; s += s);
  s.match(/./);  // Flatten string.
  assert.equal(s.length % 4, 0);
  var b = Buffer(s.length / 4 * 3);
  b.write(s, 0, s.length, 'base64');
  bench.start();
  for (var i = 0; i < 32; i += 1) b.base64Write(s, 0, s.length);
  bench.end(32);
}
