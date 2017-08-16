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
  n: [1e6],
  method: [
    'deepEqual',
    'deepStrictEqual',
    'notDeepEqual',
    'notDeepStrictEqual'
  ]
});

function main(conf) {
  const prim = primValues[conf.prim];
  const n = +conf.n;
  const actual = prim;
  const expected = prim;
  const expectedWrong = 'b';
  var i;

  // Creates new array to avoid loop invariant code motion
  switch (conf.method) {
    case 'deepEqual':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-properties
        assert.deepEqual([actual], [expected]);
      }
      bench.end(n);
      break;
    case 'deepStrictEqual':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.deepStrictEqual([actual], [expected]);
      }
      bench.end(n);
      break;
    case 'notDeepEqual':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-properties
        assert.notDeepEqual([actual], [expectedWrong]);
      }
      bench.end(n);
      break;
    case 'notDeepStrictEqual':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.notDeepStrictEqual([actual], [expectedWrong]);
      }
      bench.end(n);
      break;
    default:
      throw new Error('Unsupported method');
  }
}
