'use strict';
const common = require('../common.js');
const assert = require('assert');

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

const bench = common.createBenchmark(main, {
  prim: Object.keys(primValues),
  n: [25],
  len: [1e5],
  method: ['strict', 'nonstrict']
});

function main(conf) {
  const prim = primValues[conf.prim];
  const n = +conf.n;
  const len = +conf.len;
  const actual = [];
  const expected = [];
  var i;

  for (var x = 0; x < len; x++) {
    actual.push(prim);
    expected.push(prim);
  }

  switch (conf.method) {
    case 'strict':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-properties
        assert.deepEqual(actual, expected);
      }
      bench.end(n);
      break;
    case 'nonstrict':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.deepStrictEqual(actual, expected);
      }
      bench.end(n);
      break;
    default:
      throw new Error('Unsupported method');
  }
}
