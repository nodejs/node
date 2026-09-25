'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { createHistogram } = require('perf_hooks');

const oracle = JSON.parse(fixtures.readSync('qrde-r-oracle.json', 'utf8'));

function buildHistogram(entries, options = {}) {
  const histogram = createHistogram(options);
  for (const [value, count] of entries) {
    if (count === 1) {
      histogram.record(value);
      continue;
    }

    const block = createHistogram(options);
    block.record(value);
    let remaining = count;
    while (remaining > 0) {
      if (remaining % 2 === 1) histogram.add(block);
      remaining = Math.floor(remaining / 2);
      if (remaining > 0) block.add(block);
    }
  }
  return histogram;
}

function assertMatchesOracle(definition, result) {
  const expected = oracle.cases[definition.name];
  const span = result.quantiles.at(-1) - result.quantiles[0];
  const tolerance = Math.max(1e-10, span * definition.relativeTolerance);
  assert.strictEqual(result.bucketCount, definition.bucketCount);
  const indices = expected.indices ?? expected.probabilities.map((_, i) => i);
  if (expected.probabilities !== undefined) {
    assert.deepStrictEqual(result.probabilities,
                           new Float64Array(expected.probabilities));
  }
  for (let i = 0; i < indices.length; i++) {
    const index = indices[i];
    const difference = Math.abs(result.quantiles[index] -
                                expected.quantiles[i]);
    assert.ok(difference <= tolerance,
              `${definition.name} p${result.probabilities[index]}: ` +
              `${result.quantiles[index]} != ${expected.quantiles[i]} ` +
              `(difference ${difference}, tolerance ${tolerance})`);
  }
}

const exactTolerance = 2e-12;
const asymptoticTolerance = 2e-8;
const cases = [
  {
    name: 'small-none',
    bins: 10,
    dequantize: 'none',
    entries: [[1, 1], [2, 2], [4, 5], [16, 3], [100, 1]],
    bucketCount: 5,
    relativeTolerance: exactTolerance,
  },
  {
    name: 'small-all',
    bins: 10,
    dequantize: 'all',
    entries: [[1, 1], [2, 2], [4, 5], [16, 3], [100, 1]],
    bucketCount: 5,
    relativeTolerance: exactTolerance,
  },
  {
    name: 'exact-support',
    bins: 100,
    dequantize: 'none',
    entries: Array.from({ length: 1024 }, (_, index) => [index + 1, 1]),
    bucketCount: 1024,
    relativeTolerance: exactTolerance,
  },
  {
    name: 'tail-focused',
    probabilities: [0, 0.5, 0.9, 0.99, 0.999, 0.9999, 1],
    dequantize: 'none',
    entries: Array.from({ length: 1024 }, (_, index) => [index + 1, 1]),
    bucketCount: 1024,
    relativeTolerance: exactTolerance,
  },
  {
    name: 'approximation-threshold',
    bins: 1000,
    dequantize: 'none',
    entries: [[1, 26239], [131071, 973761]],
    options: { highest: 131071, figures: 5 },
    bucketCount: 2,
    relativeTolerance: asymptoticTolerance,
  },
  {
    name: 'multimodal-none',
    bins: 100,
    dequantize: 'none',
    entries: [[1, 900000], [10, 90000], [100, 9000], [1000, 1000]],
    bucketCount: 4,
    relativeTolerance: asymptoticTolerance,
  },
  {
    name: 'multimodal-all',
    bins: 100,
    dequantize: 'all',
    entries: [[1, 900000], [10, 90000], [100, 9000], [1000, 1000]],
    bucketCount: 4,
    relativeTolerance: asymptoticTolerance,
  },
  {
    name: 'wide-none',
    bins: 1000,
    dequantize: 'none',
    entries: [
      [2 ** 20, 700000],
      [2 ** 40, 200000],
      [2 ** 52, 99999],
      [Number.MAX_SAFE_INTEGER, 1],
    ],
    bucketCount: 4,
    relativeTolerance: asymptoticTolerance,
  },
  {
    name: 'wide-hdr',
    bins: 1000,
    dequantize: 'hdr',
    entries: [
      [2 ** 20, 700000],
      [2 ** 40, 200000],
      [2 ** 52, 99999],
      [Number.MAX_SAFE_INTEGER, 1],
    ],
    bucketCount: 4,
    relativeTolerance: asymptoticTolerance,
  },
  {
    name: 'huge-count',
    bins: 3,
    dequantize: 'none',
    entries: [[1, 2 ** 50], [1000, 2 ** 51]],
    bucketCount: 2,
    relativeTolerance: asymptoticTolerance,
  },
];

(async () => {
  assert.strictEqual(oracle.implementation, 'stats::pbeta/stats::dbeta');
  for (const definition of cases) {
    const histogram = buildHistogram(definition.entries, definition.options);
    const options = { dequantize: definition.dequantize };
    if (definition.probabilities === undefined) options.bins = definition.bins;
    else options.probabilities = definition.probabilities;
    const result = await histogram.qrde(options);
    assertMatchesOracle(definition, result);
  }
})().then(common.mustCall());
