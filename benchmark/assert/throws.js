'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  method: [
    'doesNotThrow',
    'throws',
    'throws_TypeError',
    'throws_RegExp'
  ]
});

function main(conf) {
  const n = +conf.n;
  const throws = () => { throw new TypeError('foobar'); };
  const doesNotThrow = () => { return 'foobar'; };
  const regExp = /foobar/;
  const message = 'failure';
  var i;

  switch (conf.method) {
    case 'doesNotThrow':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.doesNotThrow(doesNotThrow);
      }
      bench.end(n);
      break;
    case 'throws':
      bench.start();
      for (i = 0; i < n; ++i) {
        // eslint-disable-next-line no-restricted-syntax
        assert.throws(throws);
      }
      bench.end(n);
      break;
    case 'throws_TypeError':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.throws(throws, TypeError, message);
      }
      bench.end(n);
      break;
    case 'throws_RegExp':
      bench.start();
      for (i = 0; i < n; ++i) {
        assert.throws(throws, regExp, message);
      }
      bench.end(n);
      break;
    default:
      throw new Error(`Unsupported method ${conf.method}`);
  }
}
