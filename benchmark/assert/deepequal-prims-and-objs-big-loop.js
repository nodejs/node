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
  'new-array': new Array([1, 2, 3]),
};

const bench = common.createBenchmark(main, {
  primitive: Object.keys(primValues),
  n: [1e6],
  method: [
    'deepEqual',
    'deepStrictEqual',
    'notDeepEqual',
    'notDeepStrictEqual',
  ],
});

function main({ n, primitive, method }) {
  const prim = primValues[primitive];
  const actual = prim;
  const expected = prim;
  const expectedWrong = 'b';

  // eslint-disable-next-line no-restricted-properties
  const fn = method !== '' ? assert[method] : assert.deepEqual;
  const value2 = method.includes('not') ? expectedWrong : expected;

  bench.start();
  for (var i = 0; i < n; ++i) {
    fn([actual], [value2]);
  }
  bench.end(n);
}
