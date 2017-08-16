'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  type: [
    'Int8Array',
    'Uint8Array',
    'Int16Array',
    'Uint16Array',
    'Int32Array',
    'Uint32Array',
    'Float32Array',
    'Float64Array',
    'Uint8ClampedArray',
  ],
  n: [1],
  method: [
    'deepEqual',
    'deepStrictEqual',
    'notDeepEqual',
    'notDeepStrictEqual'
  ],
  len: [1e6]
});

function main(conf) {
  const type = conf.type;
  const clazz = global[type];
  const n = +conf.n;
  const len = +conf.len;

  const actual = new clazz(len);
  const expected = new clazz(len);
  const expectedWrong = Buffer.alloc(len);
  expectedWrong[100] = 123;
  var i;

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
