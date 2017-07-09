'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  size: [1e2, 1e3, 1e4],
  method: [
    'deepEqual',
    'deepStrictEqual',
    'notDeepEqual',
    'notDeepStrictEqual'
  ]
});

function createObj(source, add = '') {
  return source.map((n) => ({
    foo: 'yarp',
    nope: {
      bar: `123${add}`,
      a: [1, 2, 3],
      baz: n
    }
  }));
}

function main(conf) {
  const size = +conf.size;
  // TODO: Fix this "hack"
  const n = (+conf.n) / size;
  var i;

  const source = Array.apply(null, Array(size));
  const actual = createObj(source);
  const expected = createObj(source);
  const expectedWrong = createObj(source, '4');

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
