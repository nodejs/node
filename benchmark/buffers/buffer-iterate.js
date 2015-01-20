var SlowBuffer = require('buffer').SlowBuffer;
var common = require('../common.js');
var assert = require('assert');

var bench = common.createBenchmark(main, {
  size: [16, 512, 1024, 4096, 16386],
  type: ['fast', 'slow'],
  method: ['for', 'forOf'],
  n: [1e3]
});

function main(conf) {
  var len = +conf.size;
  var clazz = conf.type === 'fast' ? Buffer : SlowBuffer;
  var buffer = new clazz(len);
  buffer.fill(0);

  if (conf.method === 'for')
    benchFor(buffer, conf.n);
  else
    benchForOf(buffer, conf.n);
}


function benchFor(buffer, n) {
  bench.start();

  for (var k = 0; k < n; k++)
    for (var i = 0; i < buffer.length; i++)
      assert(buffer[i] === 0);

  bench.end(n);
}

function benchForOf(buffer, n) {
  bench.start();

  for (var k = 0; k < n; k++)
    for (var b of buffer)
      assert(b === 0);

  bench.end(n);
}
