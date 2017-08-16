'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  len: [1e2, 1e4],
  method: [
    'deepEqual',
    'deepStrictEqual',
    'notDeepEqual',
    'notDeepStrictEqual'
  ]
});

function main(conf) {
  const n = +conf.n;
  const len = +conf.len;
  var i;

  const data = Buffer.allocUnsafe(len + 1);
  const actual = Buffer.alloc(len);
  const expected = Buffer.alloc(len);
  const expectedWrong = Buffer.alloc(len + 1);
  data.copy(actual);
  data.copy(expected);
  data.copy(expectedWrong);

  switch (conf.method) {
    case 'deepEqual':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-properties
        assert.deepEqual(actual, expected);
      }
      bench.end(n);
      break;
    case 'deepStrictEqual':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.deepStrictEqual(actual, expected);
      }
      bench.end(n);
      break;
    case 'notDeepEqual':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-properties
        assert.notDeepEqual(actual, expectedWrong);
      }
      bench.end(n);
      break;
    case 'notDeepStrictEqual':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.notDeepStrictEqual(actual, expectedWrong);
      }
      bench.end(n);
      break;
    default:
      throw new Error('Unsupported method');
  }
}
