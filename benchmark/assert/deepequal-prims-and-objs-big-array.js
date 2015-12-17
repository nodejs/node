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
  n: [25]
});

function main(conf) {
  var prim = conf.prim;
  var n = +conf.n;
  var primArray;
  var primArrayCompare;
  var x;

  primArray = new Array();
  primArrayCompare = new Array();
  for (x = 0; x < (1e5); x++) {
    primArray.push(prim);
    primArrayCompare.push(prim);
  }

  bench.start();
  for (x = 0; x < n; x++) {
    assert.deepEqual(primArray, primArrayCompare);
  }
  bench.end(n);
}
