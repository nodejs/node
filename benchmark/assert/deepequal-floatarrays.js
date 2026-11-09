'use strict';
const common = require('../common.js');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  type: ['Float16Array', 'Float32Array', 'Float64Array'],
  len: [0, 1, 100, 5000],
  scenario: ['equal', 'different_first', 'different_last', 'different_length'],
  n: [10000],
}, {
  combinationFilter({ len, scenario }) {
    return len !== 0 || scenario === 'equal' || scenario === 'different_length';
  },
});

function main({ type, len, scenario, n }) {
  const clazz = globalThis[type];
  const actual = new clazz(len);
  const expected = new clazz(len + Number(scenario === 'different_length'));
  actual.fill(1.5);
  expected.fill(1.5);

  if (len > 0 && scenario === 'different_first') {
    expected[0] = 2.5;
  } else if (len > 0 && scenario === 'different_last') {
    expected[len - 1] = 2.5;
  }

  const fn = scenario === 'equal' ?
    // eslint-disable-next-line no-restricted-properties
    assert.deepEqual : assert.notDeepEqual;

  bench.start();
  for (let i = 0; i < n; i++) {
    fn(actual, expected);
  }
  bench.end(n);
}
