'use strict';
var common = require('../common.js');
var assert = require('assert');
var bench = common.createBenchmark(main, {
  prim: [
    null,
    undefined,
    'a',
    1,
    true,
    {0: 'a'},
    [1, 2, 3],
    new Array([1, 2, 3])
  ],
  n: [1e5]
});

function main(conf) {
  var prim = conf.prim;
  var n = +conf.n;
  var x;

  bench.start();

  for (x = 0; x < n; x++) {
    assert.deepEqual(new Array([prim]), new Array([prim]));
  }

  bench.end(n);
}
