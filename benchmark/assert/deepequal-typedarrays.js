'use strict';
const common = require('../common.js');
const assert = require('assert');
const bench = common.createBenchmark(main, {
  type: ('Int8Array Uint8Array Int16Array Uint16Array Int32Array Uint32Array ' +
    'Float32Array Float64Array Uint8ClampedArray').split(' '),
  n: [1],
  method: ['strict', 'nonstrict'],
  len: [1e6]
});

function main(conf) {
  const type = conf.type;
  const clazz = global[type];
  const n = +conf.n;
  const len = +conf.len;

  const actual = new clazz(len);
  const expected = new clazz(len);
  var i;

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
