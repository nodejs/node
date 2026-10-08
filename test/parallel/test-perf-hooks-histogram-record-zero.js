'use strict';

// Tests that histograms can record a value of zero.

require('../common');
const assert = require('assert');
const {
  createHistogram,
  createSlidingWindowHistogram,
  importHistogram,
} = require('perf_hooks');

{
  const h = createHistogram();
  h.record(0);
  h.record(-0);
  h.record(0n);

  assert.strictEqual(h.count, 3);
  assert.strictEqual(h.exceeds, 0);
  assert.strictEqual(h.min, 0);
  assert.strictEqual(h.minBigInt, 0n);
  assert.strictEqual(h.max, 0);
  assert.strictEqual(h.maxBigInt, 0n);
  assert.strictEqual(h.mean, 0);
  assert.strictEqual(h.stddev, 0);
  assert.strictEqual(h.percentile(50), 0);
  assert.strictEqual(h.percentileBigInt(100), 0n);
  assert.deepStrictEqual(h.percentiles, new Map([[0, 0], [100, 0]]));
}

{
  const h = createHistogram();
  h.record(5);
  // A zero recorded after a non-zero value becomes the minimum.
  h.record(0);
  h.record(0);
  h.record(3);

  assert.strictEqual(h.count, 4);
  assert.strictEqual(h.min, 0);
  assert.strictEqual(h.max, 5);
  assert.strictEqual(h.mean, 2);
  assert.strictEqual(h.countAt(0), 2);
  assert.strictEqual(h.cdf(0), 0.5);
  assert.strictEqual(h.percentile(50), 0);
  assert.strictEqual(h.percentile(75), 3);
}

{
  const h = createHistogram();
  for (const value of [-1, -1n, Number.MIN_SAFE_INTEGER, -(2n ** 63n)]) {
    assert.throws(() => h.record(value), { code: 'ERR_OUT_OF_RANGE' });
  }
  assert.strictEqual(h.count, 0);
}

{
  const h = createHistogram();
  h.recordCorrected(0, 10);
  h.recordCorrected(0n, 10n);

  assert.strictEqual(h.count, 2);
  assert.strictEqual(h.min, 0);
  assert.strictEqual(h.max, 0);

  for (const args of [[-1, 10], [-1n, 10n], [0, 0], [0n, 0n]]) {
    assert.throws(() => h.recordCorrected(...args),
                  { code: 'ERR_OUT_OF_RANGE' });
  }
  assert.strictEqual(h.count, 2);
}

{
  const a = createHistogram();
  a.record(0);
  a.record(0);
  a.record(7);

  const b = createHistogram();
  b.add(a);
  assert.strictEqual(b.count, 3);
  assert.strictEqual(b.min, 0);
  assert.strictEqual(b.max, 7);
  assert.strictEqual(b.countAt(0), 2);

  const zero = createHistogram();
  zero.record(0);

  b.subtract(zero);
  assert.strictEqual(b.count, 2);
  assert.strictEqual(b.min, 0);
  assert.strictEqual(b.countAt(0), 1);

  b.subtract(zero);
  assert.strictEqual(b.count, 1);
  assert.strictEqual(b.min, 7);
  assert.strictEqual(b.countAt(0), 0);
}

for (const values of [[0], [0, 0, 7]]) {
  const h = createHistogram();
  for (const value of values) h.record(value);

  const imported = importHistogram(h.export());
  assert.strictEqual(imported.count, values.length);
  assert.strictEqual(imported.min, 0);
  assert.strictEqual(imported.max, h.max);
  assert.strictEqual(imported.countAt(0), h.countAt(0));
  assert.deepStrictEqual(imported.percentiles, h.percentiles);
}

{
  // Values smaller than `lowest`, including zero, might not be distinguishable
  // from each other.
  const h = createHistogram({ lowest: 1000 });
  h.record(0);
  h.record(1);

  assert.strictEqual(h.count, 2);
  assert.strictEqual(h.min, 0);
  assert.strictEqual(h.countAt(0), 2);
}

{
  const histogram = createSlidingWindowHistogram({
    chunks: 2,
    recordsPerChunk: 2,
  });
  histogram.record(0);
  histogram.record(0n);
  histogram.record(3);

  const snapshot = histogram.snapshot();
  assert.strictEqual(snapshot.count, 3);
  assert.strictEqual(snapshot.min, 0);
  assert.strictEqual(snapshot.max, 3);
  assert.strictEqual(snapshot.countAt(0), 2);

  for (const value of [-1, -1n]) {
    assert.throws(() => histogram.record(value), { code: 'ERR_OUT_OF_RANGE' });
  }
  assert.strictEqual(histogram.snapshot().count, 3);
}
