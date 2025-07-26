'use strict';

const util = require('util');
const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e3],
  length: [1e3, 2e3],
  scenario: ['identical', 'small-diff', 'medium-diff', 'large-diff'],
});

function main({ n, length, scenario }) {
  const actual = Array.from({ length }, (_, i) => `${i}`);
  let expected;

  switch (scenario) {
    case 'identical': // 0% difference
      expected = Array.from({ length }, (_, i) => `${i}`);
      break;

    case 'small-diff': // ~5% difference
      expected = Array.from({ length }, (_, i) => {
        return Math.random() < 0.05 ? `modified-${i}` : `${i}`;
      });
      break;

    case 'medium-diff': // ~25% difference
      expected = Array.from({ length }, (_, i) => {
        return Math.random() < 0.25 ? `modified-${i}` : `${i}`;
      });
      break;

    case 'large-diff': // ~100% difference
      expected = Array.from({ length }, (_, i) => `modified-${i}`);
      break;
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    util.diff(actual, expected);
  }
  bench.end(n);
}
