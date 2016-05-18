/* eslint no-deepEqual: 0 */
'use strict';
var common = require('../common.js');
var assert = require('assert');

const primValues = {
  'null': null,
  'undefined': undefined,
  'string': 'a',
  'number': 1,
  'boolean': true,
  'object': { 0: 'a' },
  'array': [1, 2, 3],
  'new-array': new Array([1, 2, 3])
};

var bench = common.createBenchmark(main, {
  prim: Object.keys(primValues),
  n: [1e5]
});

function main(conf) {
  var prim = primValues[conf.prim];
  var n = +conf.n;
  var x;

  bench.start();

  for (x = 0; x < n; x++) {
    assert.deepEqual(new Array([prim]), new Array([prim]));
  }

  bench.end(n);
}
