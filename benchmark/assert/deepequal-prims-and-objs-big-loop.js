'use strict';
const common = require('../common.js');
const assert = require('assert');

const primValues = {
  'string': 'a',
  'number': 1,
  'object': { 0: 'a' },
  'array': [1, 2, 3],
};

const bench = common.createBenchmark(main, {
  primitive: Object.keys(primValues),
  n: [2e4],
  strict: [0, 1],
  method: [ 'deepEqual', 'notDeepEqual' ],
});

function main({ n, primitive, method, strict }) {
  if (!method)
    method = 'deepEqual';
  const prim = primValues[primitive];
  const actual = prim;
  const expected = prim;
  const expectedWrong = 'b';

  if (strict) {
    method = method.replace('eep', 'eepStrict');
  }
  const fn = assert[method];
  const value2 = method.includes('not') ? expectedWrong : expected;

  bench.start();
  for (var i = 0; i < n; ++i) {
    fn([actual], [value2]);
  }
  bench.end(n);
}
