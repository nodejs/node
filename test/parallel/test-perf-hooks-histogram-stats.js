// Flags: --expose-internals --no-warnings
'use strict';

require('../common');
const assert = require('assert');
const { createHistogram, importHistogram } = require('perf_hooks');

// ---------------------------------------------------------------------------
// welchTest(other) — Welch's t-test
// ---------------------------------------------------------------------------
{
  const h1 = createHistogram();
  const h2 = createHistogram();

  // Both empty → p-value 1 (no evidence of difference)
  const empty = h1.welchTest(h2);
  assert.strictEqual(empty.pValue, 1);
  assert.strictEqual(empty.tStatistic, 0);

  // Identical distributions → high p-value (not significant)
  for (let i = 0; i < 100; i++) {
    h1.record(50 + Math.ceil(Math.random() * 10));
    h2.record(50 + Math.ceil(Math.random() * 10));
  }
  const identical = h1.welchTest(h2);
  assert.strictEqual(typeof identical.tStatistic, 'number');
  assert.strictEqual(typeof identical.degreesOfFreedom, 'number');
  assert.strictEqual(typeof identical.pValue, 'number');
  assert.ok(identical.pValue >= 0 && identical.pValue <= 1);
  assert.ok(identical.degreesOfFreedom > 0);
  assert.strictEqual(typeof identical.confidenceInterval.lower, 'number');
  assert.strictEqual(typeof identical.confidenceInterval.upper, 'number');
  assert.ok(identical.confidenceInterval.lower <=
            identical.confidenceInterval.upper);

  // Very different distributions → low p-value (significant)
  const hLow = createHistogram();
  const hHigh = createHistogram();
  for (let i = 0; i < 200; i++) hLow.record(10 + Math.ceil(Math.random() * 5));
  for (let i = 0; i < 200; i++) {
    hHigh.record(1000 + Math.ceil(Math.random() * 5));
  }
  const different = hLow.welchTest(hHigh);
  assert.ok(different.pValue < 0.001,
            `Expected p < 0.001, got ${different.pValue}`);
  assert.ok(different.tStatistic < 0, 'hLow mean < hHigh mean → negative t');

  // Confidence interval should not contain 0 when significant
  assert.ok(different.confidenceInterval.upper < 0 ||
            different.confidenceInterval.lower > 0);

  // Same histogram → p-value 1
  const self = hLow.welchTest(hLow);
  assert.strictEqual(self.pValue, 1);

  // Custom confidence level
  const ci90 = hLow.welchTest(hHigh, { confidence: 0.90 });
  const ci99 = hLow.welchTest(hHigh, { confidence: 0.99 });
  // 99% CI should be wider than 90% CI
  const width90 = ci90.confidenceInterval.upper -
                  ci90.confidenceInterval.lower;
  const width99 = ci99.confidenceInterval.upper -
                  ci99.confidenceInterval.lower;
  assert.ok(width99 > width90);

  // Validation
  assert.throws(() => h1.welchTest('not a histogram'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h1.welchTest(h2, { confidence: 0 }),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h1.welchTest(h2, { confidence: 1 }),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h1.welchTest(h2, { confidence: 'high' }),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// mannWhitneyTest(other) — Mann-Whitney U test
// ---------------------------------------------------------------------------
{
  const h1 = createHistogram();
  const h2 = createHistogram();

  // Both empty → p-value 1
  const empty = h1.mannWhitneyTest(h2);
  assert.strictEqual(empty.pValue, 1);
  assert.strictEqual(empty.uStatistic, 0);
  assert.strictEqual(empty.zScore, 0);

  // Very different distributions → significant
  const hLow = createHistogram();
  const hHigh = createHistogram();
  for (let i = 0; i < 100; i++) hLow.record(1 + Math.ceil(Math.random() * 5));
  for (let i = 0; i < 100; i++) {
    hHigh.record(1000 + Math.ceil(Math.random() * 5));
  }
  const result = hLow.mannWhitneyTest(hHigh);
  assert.strictEqual(typeof result.uStatistic, 'number');
  assert.strictEqual(typeof result.zScore, 'number');
  assert.strictEqual(typeof result.pValue, 'number');
  assert.ok(result.pValue < 0.001,
            `Expected p < 0.001, got ${result.pValue}`);

  // Same histogram → p-value 1
  const self = hLow.mannWhitneyTest(hLow);
  assert.strictEqual(self.pValue, 1);

  // Identical data → high p-value
  const a = createHistogram();
  const b = createHistogram();
  for (let i = 1; i <= 50; i++) { a.record(i); b.record(i); }
  const same = a.mannWhitneyTest(b);
  assert.ok(same.pValue > 0.05,
            `Expected p > 0.05, got ${same.pValue}`);

  // Validation
  assert.throws(() => h1.mannWhitneyTest('not a histogram'),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// cohensD(other) — Cohen's d effect size
// ---------------------------------------------------------------------------
{
  const h1 = createHistogram();
  const h2 = createHistogram();

  // Both empty → 0
  assert.strictEqual(h1.cohensD(h2), 0);

  // Same histogram → 0
  for (let i = 1; i <= 100; i++) h1.record(i);
  assert.strictEqual(h1.cohensD(h1), 0);

  // Identical distributions → near 0
  const a = createHistogram();
  const b = createHistogram();
  for (let i = 0; i < 100; i++) {
    const v = 50 + Math.ceil(Math.random() * 10);
    a.record(v);
    b.record(v);
  }
  assert.ok(Math.abs(a.cohensD(b)) < 0.5);

  // Very different distributions → large |d|
  const hLow = createHistogram();
  const hHigh = createHistogram();
  for (let i = 0; i < 200; i++) hLow.record(8 + Math.ceil(Math.random() * 5));
  for (let i = 0; i < 200; i++) {
    hHigh.record(998 + Math.ceil(Math.random() * 5));
  }
  const d = hLow.cohensD(hHigh);
  assert.ok(Math.abs(d) > 1.0,
            `Expected |d| > 1, got ${d}`);
  // hLow has lower mean → d should be negative
  assert.ok(d < 0);

  // Antisymmetry: d(a,b) = -d(b,a)
  const dReverse = hHigh.cohensD(hLow);
  assert.ok(Math.abs(d + dReverse) < 1e-10);

  // Uniform variance → 0
  const u1 = createHistogram();
  const u2 = createHistogram();
  for (let i = 0; i < 100; i++) u1.record(5);
  for (let i = 0; i < 100; i++) u2.record(5);
  assert.strictEqual(u1.cohensD(u2), 0);

  // Validation
  assert.throws(() => h1.cohensD('not a histogram'),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// cliffsD(other) — Cliff's delta
// ---------------------------------------------------------------------------
{
  const h1 = createHistogram();
  const h2 = createHistogram();

  // Both empty → 0
  assert.strictEqual(h1.cliffsD(h2), 0);

  // Same histogram → 0
  for (let i = 1; i <= 100; i++) h1.record(i);
  assert.strictEqual(h1.cliffsD(h1), 0);

  // All values in h1 > all values in h2 → delta = 1
  const hHigh = createHistogram();
  const hLow = createHistogram();
  for (let i = 0; i < 100; i++) hHigh.record(1000);
  for (let i = 0; i < 100; i++) hLow.record(1);
  assert.strictEqual(hHigh.cliffsD(hLow), 1);

  // All values in h1 < all values in h2 → delta = -1
  assert.strictEqual(hLow.cliffsD(hHigh), -1);

  // Antisymmetry: d(a,b) = -d(b,a)
  const a = createHistogram();
  const b = createHistogram();
  for (let i = 0; i < 50; i++) a.record(1 + Math.ceil(Math.random() * 100));
  for (let i = 0; i < 50; i++) {
    b.record(50 + Math.ceil(Math.random() * 100));
  }
  const dAB = a.cliffsD(b);
  const dBA = b.cliffsD(a);
  assert.ok(Math.abs(dAB + dBA) < 1e-10);

  // Range check: -1 <= delta <= 1
  assert.ok(dAB >= -1 && dAB <= 1);

  // Identical data → 0
  const x = createHistogram();
  const y = createHistogram();
  for (let i = 1; i <= 50; i++) { x.record(i); y.record(i); }
  assert.strictEqual(x.cliffsD(y), 0);

  // Validation
  assert.throws(() => h1.cliffsD('not a histogram'),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// percentileCI(percentile[, options]) — percentile confidence intervals
// ---------------------------------------------------------------------------
{
  const h = createHistogram();

  // With < 2 samples, lower/upper equal value
  h.record(50);
  const one = h.percentileCI(99);
  assert.strictEqual(one.lower, one.value);
  assert.strictEqual(one.upper, one.value);

  // Fill with enough data for a meaningful CI
  for (let i = 1; i <= 1000; i++) h.record(i);
  const ci = h.percentileCI(50);
  assert.strictEqual(typeof ci.value, 'number');
  assert.strictEqual(typeof ci.lower, 'number');
  assert.strictEqual(typeof ci.upper, 'number');
  assert.ok(ci.lower <= ci.value, `lower ${ci.lower} <= value ${ci.value}`);
  assert.ok(ci.upper >= ci.value, `upper ${ci.upper} >= value ${ci.value}`);

  // 99% CI should be wider than 90% CI
  const ci90 = h.percentileCI(50, { confidence: 0.90 });
  const ci99 = h.percentileCI(50, { confidence: 0.99 });
  assert.ok((ci99.upper - ci99.lower) >= (ci90.upper - ci90.lower),
            '99% CI should be at least as wide as 90% CI');

  // Extreme percentile: p99 CI
  const ci99p = h.percentileCI(99);
  assert.ok(ci99p.lower <= ci99p.value);
  assert.ok(ci99p.upper >= ci99p.value);

  // Constant values → CI collapses to a single value
  const constant = createHistogram();
  for (let i = 0; i < 100; i++) constant.record(42);
  const constCI = constant.percentileCI(50);
  assert.strictEqual(constCI.lower, constCI.value);
  assert.strictEqual(constCI.upper, constCI.value);

  // More samples → narrower CI
  const small = createHistogram();
  const large = createHistogram();
  for (let i = 1; i <= 50; i++) { small.record(i); large.record(i); }
  for (let i = 1; i <= 950; i++) large.record(i % 50 + 1);
  const ciSmall = small.percentileCI(50);
  const ciLarge = large.percentileCI(50);
  assert.ok((ciSmall.upper - ciSmall.lower) >= (ciLarge.upper - ciLarge.lower),
            'CI should narrow with more samples');

  // Validation
  assert.throws(() => h.percentileCI(0),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.percentileCI(101),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.percentileCI('fifty'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => h.percentileCI(50, { confidence: 0 }),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.percentileCI(50, { confidence: 1 }),
                { code: 'ERR_OUT_OF_RANGE' });
}

// ---------------------------------------------------------------------------
// EWMA — exponentially weighted moving average
// ---------------------------------------------------------------------------
{
  // Without halfLife, EWMA is disabled (returns 0)
  const noEwma = createHistogram();
  for (let i = 1; i <= 100; i++) noEwma.record(i);
  assert.strictEqual(noEwma.ewmaMean, 0);
  assert.strictEqual(noEwma.ewmaStddev, 0);

  // With halfLife, EWMA tracks the smoothed mean
  const h = createHistogram({ halfLife: 10 });
  assert.strictEqual(h.ewmaMean, 0);
  assert.strictEqual(h.ewmaStddev, 0);

  // First record initializes the mean
  h.record(100);
  assert.strictEqual(h.ewmaMean, 100);
  assert.strictEqual(h.ewmaStddev, 0);

  // Record the same value repeatedly — mean should stay stable
  for (let i = 0; i < 50; i++) h.record(100);
  assert.ok(Math.abs(h.ewmaMean - 100) < 1,
            `Expected ewmaMean near 100, got ${h.ewmaMean}`);
  assert.ok(h.ewmaStddev < 1,
            `Expected near-zero stddev for constant input, got ${h.ewmaStddev}`);

  // Shift to a new value — mean should move towards it
  const meanBefore = h.ewmaMean;
  for (let i = 0; i < 100; i++) h.record(200);
  assert.ok(h.ewmaMean > meanBefore,
            'EWMA mean should increase when recording larger values');
  assert.ok(Math.abs(h.ewmaMean - 200) < 5,
            `Expected ewmaMean near 200, got ${h.ewmaMean}`);

  // Stddev should be small after converging
  for (let i = 0; i < 100; i++) h.record(200);
  assert.ok(h.ewmaStddev < 5,
            `Expected small stddev after convergence, got ${h.ewmaStddev}`);

  // Reset clears EWMA state
  h.reset();
  assert.strictEqual(h.ewmaMean, 0);
  assert.strictEqual(h.ewmaStddev, 0);

  // Shorter halfLife reacts faster
  const fast = createHistogram({ halfLife: 2 });
  const slow = createHistogram({ halfLife: 100 });
  for (let i = 0; i < 20; i++) { fast.record(100); slow.record(100); }
  for (let i = 0; i < 20; i++) { fast.record(200); slow.record(200); }
  // Fast should be closer to 200 than slow
  assert.ok(fast.ewmaMean > slow.ewmaMean,
            `fast.ewmaMean (${fast.ewmaMean}) should be > ` +
            `slow.ewmaMean (${slow.ewmaMean})`);

  // toJSON includes separate EWMA fields
  const j = createHistogram({ halfLife: 10, threshold: 50 });
  j.record(50);
  j.record(60);
  const json = j.toJSON();
  // mean/stddev are always the histogram (non-EWMA) values
  assert.strictEqual(json.mean, j.mean);
  assert.strictEqual(json.stddev, j.stddev);
  // EWMA fields are present and match getter values
  assert.strictEqual(json.ewmaMean, j.ewmaMean);
  assert.strictEqual(json.ewmaStddev, j.ewmaStddev);
  assert.strictEqual(json.ewmaErrorRate, j.ewmaErrorRate);
  assert.ok(json.ewmaMean > 0);
  assert.ok(json.ewmaErrorRate > 0);

  // toJSON still includes EWMA fields when EWMA is not enabled (all zero)
  const noEwmaJson = createHistogram();
  noEwmaJson.record(50);
  noEwmaJson.record(60);
  const json2 = noEwmaJson.toJSON();
  assert.strictEqual(json2.mean, noEwmaJson.mean);
  assert.strictEqual(json2.stddev, noEwmaJson.stddev);
  assert.strictEqual(json2.ewmaMean, 0);
  assert.strictEqual(json2.ewmaStddev, 0);
  assert.strictEqual(json2.ewmaErrorRate, 0);

  // Validation
  assert.throws(() => createHistogram({ halfLife: -1 }),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => createHistogram({ halfLife: 'ten' }),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// ewmaErrorRate / burnRate — SLO error rate tracking
// ---------------------------------------------------------------------------
{
  // Without threshold, error rate is 0
  const noThreshold = createHistogram({ halfLife: 10 });
  for (let i = 0; i < 50; i++) noThreshold.record(100);
  assert.strictEqual(noThreshold.ewmaErrorRate, 0);

  // Without halfLife, error rate is 0 even with threshold
  const noHalfLife = createHistogram({ threshold: 50 });
  for (let i = 0; i < 50; i++) noHalfLife.record(100);
  assert.strictEqual(noHalfLife.ewmaErrorRate, 0);

  // All values below threshold → error rate converges to 0
  const allGood = createHistogram({ halfLife: 10, threshold: 200 });
  for (let i = 0; i < 100; i++) allGood.record(100);
  assert.ok(allGood.ewmaErrorRate < 0.01,
            `Expected near-zero error rate, got ${allGood.ewmaErrorRate}`);

  // All values above threshold → error rate converges to 1
  const allBad = createHistogram({ halfLife: 10, threshold: 50 });
  for (let i = 0; i < 100; i++) allBad.record(100);
  assert.ok(allBad.ewmaErrorRate > 0.99,
            `Expected near-1 error rate, got ${allBad.ewmaErrorRate}`);

  // Mixed: ~50% above threshold
  const mixed = createHistogram({ halfLife: 50, threshold: 50 });
  for (let i = 0; i < 500; i++) {
    mixed.record(i % 2 === 0 ? 100 : 10);  // Alternating above/below
  }
  assert.ok(mixed.ewmaErrorRate > 0.3 && mixed.ewmaErrorRate < 0.7,
            `Expected ~0.5 error rate, got ${mixed.ewmaErrorRate}`);

  // burnRate calculation
  const h = createHistogram({ halfLife: 10, threshold: 50 });
  for (let i = 0; i < 100; i++) h.record(100);  // All exceed
  // Error rate ~1.0, SLO target 0.999 → budget 0.001 → burn rate ~1000
  const rate = h.burnRate(0.999);
  assert.ok(rate > 500,
            `Expected high burn rate, got ${rate}`);

  // When error rate is 0, burn rate is 0
  const perfect = createHistogram({ halfLife: 10, threshold: 200 });
  for (let i = 0; i < 100; i++) perfect.record(100);
  assert.ok(perfect.burnRate(0.999) < 1,
            `Expected low burn rate, got ${perfect.burnRate(0.999)}`);

  // Reset clears error rate
  h.reset();
  assert.strictEqual(h.ewmaErrorRate, 0);
  assert.strictEqual(h.burnRate(0.999), 0);

  // burnRate validation
  assert.throws(() => h.burnRate(0),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.burnRate(1),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.burnRate(NaN),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => h.burnRate('high'),
                { code: 'ERR_INVALID_ARG_TYPE' });

  // createHistogram threshold validation
  assert.throws(() => createHistogram({ threshold: -1 }),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => createHistogram({ threshold: 'high' }),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// ---------------------------------------------------------------------------
// export() / importHistogram() — CBOR round-trip
// ---------------------------------------------------------------------------
{
  // Basic round-trip
  const h = createHistogram();
  for (let i = 1; i <= 1000; i++) h.record(i);

  const buf = h.export();
  assert.ok(buf instanceof Uint8Array, 'export should return Uint8Array');
  assert.ok(buf.length > 0, 'export should not be empty');

  const h2 = importHistogram(buf);
  assert.strictEqual(h2.count, h.count);
  assert.strictEqual(h2.min, h.min);
  assert.strictEqual(h2.max, h.max);
  assert.strictEqual(h2.mean, h.mean);
  assert.strictEqual(h2.stddev, h.stddev);
  assert.strictEqual(h2.percentile(50), h.percentile(50));
  assert.strictEqual(h2.percentile(99), h.percentile(99));
  assert.strictEqual(h2.percentile(99.9), h.percentile(99.9));

  // Imported histogram is recordable
  h2.record(9999);
  assert.strictEqual(h2.count, h.count + 1);

  // Round-trip with EWMA and threshold
  const h3 = createHistogram({ halfLife: 10, threshold: 500 });
  for (let i = 1; i <= 200; i++) h3.record(i);
  const buf3 = h3.export();
  const h4 = importHistogram(buf3);
  assert.strictEqual(h4.count, h3.count);
  assert.strictEqual(h4.ewmaMean, h3.ewmaMean);
  assert.strictEqual(h4.ewmaStddev, h3.ewmaStddev);
  assert.strictEqual(h4.ewmaErrorRate, h3.ewmaErrorRate);

  // Empty histogram round-trip
  const empty = createHistogram();
  const emptyBuf = empty.export();
  const empty2 = importHistogram(emptyBuf);
  assert.strictEqual(empty2.count, 0);
  assert.strictEqual(empty2.min, 9223372036854776000);  // INT64_MAX as double

  // Sparse: only a few distinct values
  const sparse = createHistogram();
  sparse.record(1);
  sparse.record(1000000);
  const sparseBuf = sparse.export();
  const sparse2 = importHistogram(sparseBuf);
  assert.strictEqual(sparse2.count, 2);
  assert.strictEqual(sparse2.percentile(1), sparse.percentile(1));
  assert.strictEqual(sparse2.percentile(100), sparse.percentile(100));

  // Size scales with distinct values, not total bucket count
  assert.ok(sparseBuf.length < 200,
            `Sparse export should be small, got ${sparseBuf.length}`);

  // Validation — type and format
  assert.throws(() => importHistogram('not a uint8array'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => importHistogram(new Uint8Array(0)),
                { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(() => importHistogram(new Uint8Array([0xff, 0xff])),
                { code: 'ERR_INVALID_ARG_VALUE' });

  // --- hdr_init failures (invalid histogram options) ---

  // lowest=0 violates lowest>=1.
  assert.throws(() => importHistogram(new Uint8Array([0xa1, 0x01, 0x00])),
                { code: 'ERR_INVALID_ARG_VALUE' });
  // figures=0 violates figures>=1.
  assert.throws(() => importHistogram(new Uint8Array([0xa1, 0x03, 0x00])),
                { code: 'ERR_INVALID_ARG_VALUE' });
  // figures=6 violates figures<=5.
  assert.throws(() => importHistogram(new Uint8Array([0xa1, 0x03, 0x06])),
                { code: 'ERR_INVALID_ARG_VALUE' });
  // lowest=100, highest=100: lowest*2 > highest.
  assert.throws(() => importHistogram(new Uint8Array([
    0xa2,                                         // map(2)
    0x01, 0x18, 100,                              // 1 (lowest) = 100
    0x02, 0x18, 100,                              // 2 (highest) = 100
  ])), { code: 'ERR_INVALID_ARG_VALUE' });
  // Large values that trigger hdr_init internal overflow
  // (unit_magnitude + sub_bucket_half_count_magnitude > 61).
  assert.throws(() => importHistogram(new Uint8Array([
    0xa3,                                         // map(3)
    0x01, 0x1b, 0, 0, 0x20, 0, 0, 0, 0, 0,       // 1 (lowest) = 2**45
    0x02, 0x1b, 0, 0, 0x40, 0, 0, 0, 0, 0,       // 2 (highest) = 2**46
    0x03, 0x05,                                   // 3 (figures) = 5
  ])), { code: 'ERR_INVALID_ARG_VALUE' });

  // --- Structural CBOR validation ---

  // Wrong version number (version=99).
  assert.throws(() => importHistogram(new Uint8Array([
    0xa1,                                         // map(1)
    0x00, 0x18, 99,                               // 0 (version) = 99
  ])), { code: 'ERR_INVALID_ARG_VALUE' });
  // Unknown top-level key (key=255).
  assert.throws(() => importHistogram(new Uint8Array([
    0xa1,                                         // map(1)
    0x18, 0xff, 0x00,                             // 255 (unknown) = 0
  ])), { code: 'ERR_INVALID_ARG_VALUE' });
  // Counts array with odd length (must be even: delta/count pairs).
  assert.throws(() => importHistogram(new Uint8Array([
    0xa1,                                         // map(1)
    0x0a, 0x81, 0x01,                             // 10 (counts) = [1]
  ])), { code: 'ERR_INVALID_ARG_VALUE' });

  // --- Post-construction validation ---

  // counts_len mismatch: valid options but declared counts_len
  // doesn't match what hdr_init actually produces.
  assert.throws(() => importHistogram(new Uint8Array([
    0xa2,                                         // map(2)
    0x09, 0x01,                                   // 9 (countsLen) = 1
    0x0a, 0x80,                                   // 10 (counts) = []
  ])), { code: 'ERR_INVALID_ARG_VALUE' });
  // Oversized counts array: declared length exceeds remaining buffer.
  // Without bounds checking, reserve() would OOM-crash.
  assert.throws(() => importHistogram(new Uint8Array([
    0xa1,                                         // map(1)
    0x0a, 0x9b,                                   // 10 (counts) = array(
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //   2**52 elements)
  ])), { code: 'ERR_INVALID_ARG_VALUE' });
  // Sparse count index out of bounds: lowest=1, highest=100, figures=1
  // produces counts_len=64. Index 100 exceeds it.
  assert.throws(() => importHistogram(new Uint8Array([
    0xa4,                                         // map(4)
    0x02, 0x18, 0x64,                             // 2 (highest) = 100
    0x03, 0x01,                                   // 3 (figures) = 1
    0x09, 0x18, 0x40,                             // 9 (countsLen) = 64
    0x0a, 0x82, 0x18, 0x64, 0x01,                 // 10 (counts) = [100, 1]
  ])), { code: 'ERR_INVALID_ARG_VALUE' });
}

// ---------------------------------------------------------------------------
// ERR_INVALID_THIS for all new methods on wrong receiver
// ---------------------------------------------------------------------------
{
  const { Histogram } = require('internal/histogram');
  const h = createHistogram();
  for (let i = 1; i <= 10; i++) h.record(i);
  const wrongThis = {};

  const methods = [
    ['welchTest', [h]],
    ['mannWhitneyTest', [h]],
    ['cohensD', [h]],
    ['cliffsD', [h]],
    ['percentileCI', [50]],
    ['burnRate', [0.999]],
  ];

  for (const [method, args] of methods) {
    assert.throws(
      () => Histogram.prototype[method].call(wrongThis, ...args),
      { code: 'ERR_INVALID_THIS' },
      `${method} should throw ERR_INVALID_THIS`,
    );
  }

  // Getter properties
  const getters = ['ewmaMean', 'ewmaStddev', 'ewmaErrorRate'];
  for (const getter of getters) {
    const desc = Object.getOwnPropertyDescriptor(Histogram.prototype, getter);
    assert.throws(
      () => desc.get.call(wrongThis),
      { code: 'ERR_INVALID_THIS' },
      `${getter} should throw ERR_INVALID_THIS`,
    );
  }
}

// ---------------------------------------------------------------------------
// Undefined return when kHandle is missing native methods
// ---------------------------------------------------------------------------
{
  const {
    Histogram,
    kHandle,
    kSkipThrow,
  } = require('internal/histogram');
  const h = createHistogram();
  for (let i = 1; i <= 10; i++) h.record(i);

  // Create a histogram instance with a null handle. This passes
  // isHistogram() (null !== undefined) but the optional chaining
  // (this[kHandle]?.method()) short-circuits to undefined.
  const stub = new Histogram(kSkipThrow);
  stub[kHandle] = null;

  assert.strictEqual(stub.welchTest(h), undefined);
  assert.strictEqual(stub.mannWhitneyTest(h), undefined);
  assert.strictEqual(stub.percentileCI(50), undefined);
  assert.strictEqual(stub.burnRate(0.999), undefined);
}

// ---------------------------------------------------------------------------
// Fast API path coverage for EWMA getters
// ---------------------------------------------------------------------------
{
  const h = createHistogram({ halfLife: 10, threshold: 50 });
  for (let i = 1; i <= 100; i++) h.record(i);

  // Call in a tight loop to trigger V8 fast-path optimization.
  function readEwma(histogram, iterations) {
    let mean = 0;
    let stddev = 0;
    let errorRate = 0;
    for (let i = 0; i < iterations; i++) {
      mean = histogram.ewmaMean;
      stddev = histogram.ewmaStddev;
      errorRate = histogram.ewmaErrorRate;
    }
    return { mean, stddev, errorRate };
  }

  const result = readEwma(h, 1e4);
  assert.strictEqual(typeof result.mean, 'number');
  assert.ok(result.mean > 0);
  assert.strictEqual(typeof result.stddev, 'number');
  assert.ok(result.stddev > 0);
  assert.strictEqual(typeof result.errorRate, 'number');
  assert.ok(result.errorRate > 0);
}

// ---------------------------------------------------------------------------
// Cross-consistency: when welchTest is significant, cohensD should
// indicate a non-trivial effect, and cliffsD should agree on direction.
// ---------------------------------------------------------------------------
{
  const baseline = createHistogram();
  const regressed = createHistogram();
  for (let i = 0; i < 500; i++) {
    baseline.record(10 + Math.ceil(Math.random() * 20));
  }
  for (let i = 0; i < 500; i++) {
    regressed.record(50 + Math.ceil(Math.random() * 20));
  }

  const welch = baseline.welchTest(regressed);
  const d = baseline.cohensD(regressed);
  const cliff = baseline.cliffsD(regressed);

  // Should be highly significant
  assert.ok(welch.pValue < 0.001);
  // Cohen's d should indicate a large effect (|d| > 0.8)
  assert.ok(Math.abs(d) > 0.8);
  // Cliff's delta should indicate baseline < regressed
  assert.ok(cliff < -0.5);
  // All three agree on the direction
  assert.ok(d < 0);  // Baseline mean < regressed mean
  assert.ok(welch.tStatistic < 0);
}
