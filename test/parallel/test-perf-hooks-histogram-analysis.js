// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');
const assert = require('assert');
const { createHistogram } = require('perf_hooks');
const { internalBinding } = require('internal/test/binding');
const { inspect } = require('util');

// ---------------------------------------------------------------------------
// cdf(value) — cumulative distribution function
// ---------------------------------------------------------------------------
{
  const h = createHistogram();

  // Empty histogram returns 0
  assert.strictEqual(h.cdf(1), 0);

  for (let i = 1; i <= 5; i++) h.record(i);

  // Below min → 0
  assert.strictEqual(h.cdf(0), 0);

  // At or above some values → monotonically increasing
  assert.ok(h.cdf(1) > 0);
  assert.ok(h.cdf(3) >= h.cdf(1));
  assert.ok(h.cdf(5) >= h.cdf(3));

  // Well above max → 1.0
  assert.strictEqual(h.cdf(1000000), 1.0);

  // Validation
  assert.throws(() => h.cdf('hello'), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h.cdf(), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h.cdf(undefined), { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// ccdf(value) — complementary CDF = 1 - cdf
// ---------------------------------------------------------------------------
{
  const h = createHistogram();

  // Empty: cdf=0 so ccdf=1
  assert.strictEqual(h.ccdf(1), 1);

  for (let i = 1; i <= 5; i++) h.record(i);

  // CCDF + CDF === 1 for all values
  for (const v of [0, 1, 3, 5, 1000000]) {
    const sum = h.ccdf(v) + h.cdf(v);
    assert.ok(Math.abs(sum - 1) < 1e-10, `ccdf(${v})+cdf(${v})=${sum}`);
  }

  // Well above max → 0
  assert.strictEqual(h.ccdf(1000000), 0);

  // Validation
  assert.throws(() => h.ccdf('hello'), { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// countAt(value) — count in equivalent bucket
// ---------------------------------------------------------------------------
{
  const h = createHistogram();

  // Empty → 0
  assert.strictEqual(h.countAt(1), 0);

  h.record(1);
  h.record(1);
  h.record(1);
  h.record(100);

  assert.strictEqual(h.countAt(1), 3);
  assert.strictEqual(h.countAt(100), 1);
  assert.strictEqual(h.countAt(999999), 0);

  // Validation
  assert.throws(() => h.countAt('hello'), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h.countAt(), { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// skewness getter
// ---------------------------------------------------------------------------
{
  const h = createHistogram();

  // Too few values returns 0
  assert.strictEqual(h.skewness, 0);
  h.record(1);
  assert.strictEqual(h.skewness, 0);
  h.record(2);
  assert.strictEqual(h.skewness, 0);

  // With 3+ values, returns a number
  h.record(3);
  assert.strictEqual(typeof h.skewness, 'number');
  assert.ok(!Number.isNaN(h.skewness));

  // Right-skewed distribution → positive skewness
  const right = createHistogram();
  for (let i = 0; i < 100; i++) right.record(1);
  for (let i = 0; i < 10; i++) right.record(10000);
  assert.ok(right.skewness > 0);

  // Appears in inspect output
  assert.ok(inspect(right, { depth: null }).includes('skewness'));

  // Appears in toJSON
  const json = right.toJSON();
  assert.ok('skewness' in json);
  assert.strictEqual(typeof json.skewness, 'number');

  // Uniform distribution: zero stddev → returns 0
  const uniform = createHistogram();
  for (let i = 0; i < 10; i++) uniform.record(1);
  assert.strictEqual(uniform.skewness, 0);
}

// ---------------------------------------------------------------------------
// kurtosis getter
// ---------------------------------------------------------------------------
{
  const h = createHistogram();

  // Too few values returns 0
  assert.strictEqual(h.kurtosis, 0);
  h.record(1);
  h.record(2);
  h.record(3);
  assert.strictEqual(h.kurtosis, 0);

  // With 4+ values, returns a number
  h.record(4);
  assert.strictEqual(typeof h.kurtosis, 'number');
  assert.ok(!Number.isNaN(h.kurtosis));

  // Appears in inspect and toJSON
  const h2 = createHistogram();
  for (let i = 1; i <= 100; i++) h2.record(i);
  assert.ok(inspect(h2, { depth: null }).includes('kurtosis'));
  const json = h2.toJSON();
  assert.ok('kurtosis' in json);
  assert.strictEqual(typeof json.kurtosis, 'number');

  // Uniform distribution: zero stddev → returns 0
  const uniform = createHistogram();
  for (let i = 0; i < 10; i++) uniform.record(1);
  assert.strictEqual(uniform.kurtosis, 0);
}

// ---------------------------------------------------------------------------
// ksTest(other) — Kolmogorov-Smirnov D-statistic
// ---------------------------------------------------------------------------
{
  const h1 = createHistogram();
  const h2 = createHistogram();

  // Both empty → 0
  assert.strictEqual(h1.ksTest(h2), 0);

  // Identical distributions → 0
  for (let i = 1; i <= 100; i++) { h1.record(i); h2.record(i); }
  assert.strictEqual(h1.ksTest(h2), 0);

  // Same histogram against itself → 0
  assert.strictEqual(h1.ksTest(h1), 0);

  // Different distributions → D > 0
  const h3 = createHistogram();
  for (let i = 1000; i <= 2000; i++) h3.record(i);
  const d = h1.ksTest(h3);
  assert.ok(d > 0);
  assert.ok(d <= 1);

  // Symmetry: D(a,b) === D(b,a)
  assert.strictEqual(h1.ksTest(h3), h3.ksTest(h1));

  // Completely disjoint → D close to 1
  const hLow = createHistogram();
  const hHigh = createHistogram();
  for (let i = 0; i < 100; i++) hLow.record(1);
  for (let i = 0; i < 100; i++) hHigh.record(100000);
  assert.ok(hLow.ksTest(hHigh) > 0.9);

  // One empty → 0
  const empty = createHistogram();
  assert.strictEqual(h1.ksTest(empty), 0);

  // Validation: non-histogram throws
  assert.throws(() => h1.ksTest('not a histogram'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h1.ksTest(42),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h1.ksTest({}),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// percentilesAt(percentiles) — batch percentile query
// ---------------------------------------------------------------------------
{
  const h = createHistogram();
  for (let i = 1; i <= 100; i++) h.record(i);

  // Returns a Map
  const result = h.percentilesAt([50, 90, 99]);
  assert.ok(result instanceof Map);
  assert.strictEqual(result.size, 3);

  // Keys are the requested percentiles
  assert.ok(result.has(50));
  assert.ok(result.has(90));
  assert.ok(result.has(99));

  // Values match individual percentile() calls
  assert.strictEqual(result.get(50), h.percentile(50));
  assert.strictEqual(result.get(90), h.percentile(90));
  assert.strictEqual(result.get(99), h.percentile(99));

  // Single element
  const single = h.percentilesAt([50]);
  assert.strictEqual(single.size, 1);

  // Unsorted input still works (internally sorted)
  const unsorted = h.percentilesAt([99, 50, 90]);
  assert.strictEqual(unsorted.get(50), h.percentile(50));

  // Validation
  assert.throws(() => h.percentilesAt('not array'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h.percentilesAt([0]),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.percentilesAt([101]),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.percentilesAt([NaN]),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.percentilesAt([-1]),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.percentilesAt(['hello']),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// linearBuckets(stepSize) — linearly-spaced bucket iteration
// ---------------------------------------------------------------------------
{
  const h = createHistogram();
  for (let i = 1; i <= 100; i++) h.record(i);

  const buckets = h.linearBuckets(10);
  assert.ok(buckets instanceof Map);
  assert.ok(buckets.size > 0);

  // All keys and values are numbers
  for (const [key, value] of buckets) {
    assert.strictEqual(typeof key, 'number');
    assert.strictEqual(typeof value, 'number');
    assert.ok(value >= 0);
  }

  // Total count across buckets equals histogram count
  let total = 0;
  for (const [, count] of buckets) total += count;
  assert.strictEqual(total, h.count);

  // Different step sizes produce different bucket counts
  const finer = h.linearBuckets(5);
  assert.ok(finer.size >= buckets.size);

  // Validation
  assert.throws(() => h.linearBuckets(0), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.linearBuckets(-1), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.linearBuckets('hello'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h.linearBuckets(1.5), { code: 'ERR_OUT_OF_RANGE' });
}

// ---------------------------------------------------------------------------
// logBuckets(firstBucket, base) — logarithmically-spaced bucket iteration
// ---------------------------------------------------------------------------
{
  const h = createHistogram();
  for (let i = 1; i <= 1000; i++) h.record(i);

  const buckets = h.logBuckets(1, 2);
  assert.ok(buckets instanceof Map);
  assert.ok(buckets.size > 0);

  for (const [key, value] of buckets) {
    assert.strictEqual(typeof key, 'number');
    assert.strictEqual(typeof value, 'number');
    assert.ok(value >= 0);
  }

  // Total count across buckets equals histogram count
  let total = 0;
  for (const [, count] of buckets) total += count;
  assert.strictEqual(total, h.count);

  // Validation
  assert.throws(() => h.logBuckets(0, 2), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.logBuckets(-1, 2), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.logBuckets(1, 1), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.logBuckets(1, 0.5), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.logBuckets(1, -2), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.logBuckets('hello', 2),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h.logBuckets(1, 'hello'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h.logBuckets(1.5, 2), { code: 'ERR_OUT_OF_RANGE' });
}

// ---------------------------------------------------------------------------
// subtract(other) — subtract histogram counts
// ---------------------------------------------------------------------------
{
  const h1 = createHistogram();
  const h2 = createHistogram();

  for (let i = 1; i <= 10; i++) h1.record(i);
  for (let i = 1; i <= 5; i++) h2.record(i);

  const countBefore = h1.count;
  h1.subtract(h2);

  // Count should decrease
  assert.ok(h1.count < countBefore);

  // Subtracting from self zeros out
  const h3 = createHistogram();
  for (let i = 1; i <= 10; i++) h3.record(i);
  h3.subtract(h3);
  assert.strictEqual(h3.count, 0);

  // Clamping: subtracting more than present doesn't go negative
  const hSmall = createHistogram();
  const hBig = createHistogram();
  hSmall.record(1);
  for (let i = 0; i < 100; i++) hBig.record(1);
  hSmall.subtract(hBig);
  assert.strictEqual(hSmall.count, 0);

  // Validation
  assert.throws(() => h1.subtract('not a histogram'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h1.subtract(42),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h1.subtract({}),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// recordCorrected(val, expectedInterval) — coordinated omission correction
// ---------------------------------------------------------------------------
{
  // Basic recording with number args
  const h = createHistogram();
  h.recordCorrected(100, 10);
  assert.ok(h.count > 0);

  // Should record more values than a plain record (backfilling)
  const hPlain = createHistogram();
  hPlain.record(100);
  assert.ok(h.count > hPlain.count);

  // BigInt variant
  const hBig = createHistogram();
  hBig.recordCorrected(100n, 10n);
  assert.ok(hBig.count > 0);

  // Mixed types should throw (bigint val, number interval)
  assert.throws(() => h.recordCorrected(100n, 10),
                { code: 'ERR_INVALID_ARG_TYPE' });

  // Validation: non-integer
  assert.throws(() => h.recordCorrected('hello', 10),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h.recordCorrected(100, 'hello'),
                { code: 'ERR_INVALID_ARG_TYPE' });

  // Out of range
  assert.throws(() => h.recordCorrected(0, 10),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.recordCorrected(100, 0),
                { code: 'ERR_OUT_OF_RANGE' });
}

// ---------------------------------------------------------------------------
// ERR_INVALID_THIS for all new methods on wrong receiver
// ---------------------------------------------------------------------------
{
  const { Histogram } = require('internal/histogram');
  const h = createHistogram();
  const wrongThis = {};

  // Methods
  const methods = [
    ['cdf', [1]],
    ['ccdf', [1]],
    ['countAt', [1]],
    ['ksTest', [h]],
    ['linearBuckets', [10]],
    ['logBuckets', [1, 2]],
    ['percentilesAt', [[50]]],
  ];

  for (const [method, args] of methods) {
    assert.throws(
      () => Histogram.prototype[method].call(wrongThis, ...args),
      { code: 'ERR_INVALID_THIS' },
      `${method} should throw ERR_INVALID_THIS`
    );
  }

  // Getters
  for (const getter of ['skewness', 'kurtosis']) {
    const desc = Object.getOwnPropertyDescriptor(
      Histogram.prototype, getter);
    assert.throws(
      () => desc.get.call(wrongThis),
      { code: 'ERR_INVALID_THIS' },
      `${getter} getter should throw ERR_INVALID_THIS`
    );
  }
}

// ---------------------------------------------------------------------------
// Empty histogram edge cases
// ---------------------------------------------------------------------------
{
  const h = createHistogram();

  assert.strictEqual(h.cdf(1), 0);
  assert.strictEqual(h.ccdf(1), 1);
  assert.strictEqual(h.countAt(1), 0);
  assert.strictEqual(h.skewness, 0);
  assert.strictEqual(h.kurtosis, 0);

  const empty2 = createHistogram();
  assert.strictEqual(h.ksTest(empty2), 0);

  const pctAt = h.percentilesAt([50, 99]);
  assert.ok(pctAt instanceof Map);
  assert.strictEqual(pctAt.size, 2);

  const linear = h.linearBuckets(10);
  assert.ok(linear instanceof Map);

  const log = h.logBuckets(1, 2);
  assert.ok(log instanceof Map);
}

// ---------------------------------------------------------------------------
// Single-value histogram edge cases
// ---------------------------------------------------------------------------
{
  const h = createHistogram();
  h.record(42);

  assert.strictEqual(h.skewness, 0);  // Needs >= 3
  assert.strictEqual(h.kurtosis, 0);  // Needs >= 4
  assert.strictEqual(h.cdf(42), 1);
  assert.strictEqual(h.cdf(1), 0);
  assert.strictEqual(h.ccdf(42), 0);
  assert.strictEqual(h.countAt(42), 1);
}

// ---------------------------------------------------------------------------
// Fast API call tests for new methods
// ---------------------------------------------------------------------------
{
  const h = createHistogram();
  h.record(1);
  h.record(100);

  // Prepare cdf and countAt methods for optimization
  eval('%PrepareFunctionForOptimization(h.cdf)');
  eval('%PrepareFunctionForOptimization(h.countAt)');

  // Warmup call
  h.cdf(50);
  h.countAt(1);

  // Optimize
  eval('%OptimizeFunctionOnNextCall(h.cdf)');
  eval('%OptimizeFunctionOnNextCall(h.countAt)');

  // Fast-path call
  h.cdf(50);
  h.countAt(1);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('histogram.cdf'), 1);
    assert.strictEqual(getV8FastApiCallCount('histogram.countAt'), 1);
  }
}
