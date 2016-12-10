'use strict';
var common = require('../common.js');
var assert = require('assert');
var bench = common.createBenchmark(main, {
  type: ('Int8Array Uint8Array Int16Array Uint16Array Int32Array Uint32Array ' +
    'Float32Array Float64Array Uint8ClampedArray').split(' '),
  n: [1]
});

function main(conf) {
  var type = conf.type;
  var clazz = global[type];
  var n = +conf.n;

  bench.start();
  var actual = new clazz(n * 1e6);
  var expected = new clazz(n * 1e6);

  // eslint-disable-next-line no-restricted-properties
  assert.deepEqual(actual, expected);

  bench.end(n);
}
