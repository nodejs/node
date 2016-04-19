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
  n: [25]
});

function main(conf) {
  var prim = primValues[conf.prim];
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
