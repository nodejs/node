'use strict';

const common = require('../common');
const assert = require('assert');
const { createHistogram } = require('perf_hooks');

function assertClose(actual, expected, tolerance = 1e-12) {
  assert.ok(Math.abs(actual - expected) <= tolerance,
            `${actual} != ${expected}`);
}

function recordRepeated(histogram, options, value, count) {
  const block = createHistogram(options);
  block.record(value);
  while (count > 0) {
    if (count % 2 === 1) histogram.add(block);
    count = Math.floor(count / 2);
    if (count > 0) block.add(block);
  }
}

(async () => {
  const empty = createHistogram();
  const emptyResult = await empty.qrde();
  assert.strictEqual(Object.getPrototypeOf(emptyResult), null);
  const defaultProbabilities = new Float64Array(101);
  for (let i = 0; i <= 100; i++) defaultProbabilities[i] = i / 100;
  assert.deepStrictEqual(emptyResult.probabilities, defaultProbabilities);
  assert.deepStrictEqual(emptyResult.quantiles, new Float64Array());
  assert.deepStrictEqual(emptyResult.densities, new Float64Array());
  assert.strictEqual(emptyResult.count, 0n);
  assert.strictEqual(emptyResult.bucketCount, 0);
  assert.strictEqual(emptyResult.corrections, 0);
  assert.strictEqual(emptyResult.dequantize, 'hdr');

  assert.throws(() => empty.qrde.call({}), {
    code: 'ERR_INVALID_THIS',
  });
  assert.throws(() => empty.qrde(null), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => empty.qrde({ bins: 0 }), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => empty.qrde({ bins: 1001 }), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => empty.qrde({
    bins: 10,
    probabilities: [0, 1],
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => empty.qrde({ probabilities: [0] }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => empty.qrde({ probabilities: [0.1, 1] }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => empty.qrde({ probabilities: [0, 0.9] }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => empty.qrde({ probabilities: [0, 0.5, 0.5, 1] }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => empty.qrde({ probabilities: [0, 1.1, 1] }), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => empty.qrde({ probabilities: new Array(1002) }), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => empty.qrde({ dequantize: true }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => empty.qrde({ cache: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  const sample = createHistogram();
  sample.record(3);
  sample.record(4);
  sample.record(7);
  const pending = sample.qrde({ bins: 10, dequantize: 'none' });
  assert.ok(pending instanceof Promise);
  const result = await pending;
  assert.ok(result.quantiles instanceof Float64Array);
  assert.ok(result.densities instanceof Float64Array);
  const tenBinProbabilities = new Float64Array(11);
  for (let i = 0; i <= 10; i++) tenBinProbabilities[i] = i / 10;
  assert.deepStrictEqual(result.probabilities, tenBinProbabilities);
  assert.strictEqual(result.quantiles.length, 11);
  assert.strictEqual(result.densities.length, 10);
  assert.strictEqual(result.count, 3n);
  assert.strictEqual(result.bucketCount, 3);
  assert.strictEqual(result.corrections, 0);
  assert.strictEqual(result.dequantize, 'none');
  assertClose(result.quantiles[5], 122 / 27);
  for (let i = 0; i < result.densities.length; i++) {
    const width = result.quantiles[i + 1] - result.quantiles[i];
    assertClose(result.densities[i] * width, 0.1);
  }

  const customInput = [0, 0.2, 0.5, 0.9, 1];
  const customPending = sample.qrde({
    probabilities: customInput,
    dequantize: 'none',
  });
  customInput[1] = 0.4;
  const custom = await customPending;
  assert.deepStrictEqual(custom.probabilities,
                         new Float64Array([0, 0.2, 0.5, 0.9, 1]));
  assert.strictEqual(custom.quantiles.length, 5);
  assert.strictEqual(custom.densities.length, 4);
  assertClose(custom.quantiles[1], result.quantiles[2]);
  assertClose(custom.quantiles[2], result.quantiles[5]);
  assertClose(custom.quantiles[3], result.quantiles[9]);
  for (let i = 0; i < custom.densities.length; i++) {
    const width = custom.quantiles[i + 1] - custom.quantiles[i];
    const mass = custom.probabilities[i + 1] - custom.probabilities[i];
    assertClose(custom.densities[i] * width, mass);
  }

  const pointMass = createHistogram();
  for (let i = 0; i < 100; i++) pointMass.record(100);

  const pointMassNone =
    await pointMass.qrde({ bins: 10, dequantize: 'none' });
  const pointMassHdr = await pointMass.qrde({ bins: 10 });
  assert.deepStrictEqual(pointMassNone.quantiles,
                         new Float64Array(11).fill(100));
  assert.deepStrictEqual(pointMassHdr.quantiles, pointMassNone.quantiles);
  assert.ok(pointMassNone.densities.every((value) => value === Infinity));
  assert.ok(pointMassHdr.densities.every((value) => value === Infinity));

  const pointMassAll =
    await pointMass.qrde({ bins: 10, dequantize: 'all' });
  for (let i = 0; i <= 10; i++) {
    assertClose(pointMassAll.quantiles[i], 99.5 + i / 10);
  }
  for (const density of pointMassAll.densities) assertClose(density, 1);
  assert.strictEqual(pointMassAll.corrections, 0);

  const wideBucket = createHistogram();
  for (let i = 0; i < 100; i++) wideBucket.record(100_000);
  const [wideNone, wideHdr, wideAll] = await Promise.all([
    wideBucket.qrde({ bins: 10, dequantize: 'none' }),
    wideBucket.qrde({ bins: 10, dequantize: 'hdr' }),
    wideBucket.qrde({ bins: 10, dequantize: 'all' }),
  ]);
  assert.ok(wideNone.densities.every((value) => value === Infinity));
  assert.ok(wideHdr.densities.every(Number.isFinite));
  assert.deepStrictEqual(wideHdr.quantiles, wideAll.quantiles);

  const snapshot = createHistogram();
  snapshot.record(3);
  snapshot.record(4);
  snapshot.record(7);
  const snapshotPending =
    snapshot.qrde({ bins: 1000, dequantize: 'none' });
  snapshot.record(1000);
  const snapshotResult = await snapshotPending;
  assert.strictEqual(snapshotResult.count, 3n);
  assert.strictEqual(snapshotResult.quantiles.at(-1), 7);

  const cachedSnapshot = createHistogram();
  cachedSnapshot.record(3);
  cachedSnapshot.record(4);
  cachedSnapshot.record(7);
  const cachedPending = cachedSnapshot.qrde({ bins: 1000, cache: true });
  cachedSnapshot.record(1000);
  const cachedResult = await cachedPending;
  assert.strictEqual(cachedResult.count, 3n);
  assert.strictEqual(cachedResult.quantiles.at(-1), 7);
  const refreshedCache = await cachedSnapshot.qrde({
    probabilities: [0, 0.5, 1],
    cache: true,
  });
  assert.strictEqual(refreshedCache.count, 4n);
  assert.strictEqual(refreshedCache.quantiles.at(-1), 1000);
  const reusedCache = await cachedSnapshot.qrde({ bins: 4, cache: true });
  assert.strictEqual(reusedCache.count, 4n);
  assert.strictEqual(reusedCache.quantiles.at(-1), 1000);
  cachedSnapshot.reset();
  const invalidatedCache = await cachedSnapshot.qrde({ cache: true });
  assert.strictEqual(invalidatedCache.count, 0n);

  const arithmeticCache = createHistogram();
  arithmeticCache.record(1);
  await arithmeticCache.qrde({ cache: true });
  const operand = createHistogram();
  operand.record(2);
  arithmeticCache.add(operand);
  assert.strictEqual(
    (await arithmeticCache.qrde({ cache: true })).count, 2n);
  arithmeticCache.subtract(operand);
  assert.strictEqual(
    (await arithmeticCache.qrde({ cache: true })).count, 1n);
  arithmeticCache.recordCorrected(10, 5);
  assert.strictEqual(
    (await arithmeticCache.qrde({ cache: true })).count, 3n);

  const exactTails = createHistogram();
  for (let i = 1; i <= 1024; i++) exactTails.record(i);
  const [exactNone, exactAll] = await Promise.all([
    exactTails.qrde({ bins: 100, dequantize: 'none' }),
    exactTails.qrde({ bins: 100, dequantize: 'all' }),
  ]);
  assert.strictEqual(exactNone.bucketCount, 1024);
  assertClose(exactNone.quantiles[1], 10.740000001653616, 1e-9);
  assertClose(exactNone.quantiles[99], 1014.2599999983464, 1e-9);
  assertClose(exactAll.quantiles[1], exactNone.quantiles[1]);
  assertClose(exactAll.quantiles[99], exactNone.quantiles[99]);

  const tinyUpperTail = createHistogram({ highest: 32768, figures: 4 });
  for (let i = 1; i <= 31; i++) tinyUpperTail.record(i);
  tinyUpperTail.record(16384);
  const tinyUpperTailResult =
    await tinyUpperTail.qrde({ bins: 2, dequantize: 'none' });
  assertClose(tinyUpperTailResult.quantiles[1],
              16.500000000000938, 2e-13);

  const largeCount = createHistogram();
  largeCount.record(1);
  largeCount.record(3);
  for (let i = 0; i < 52; i++) {
    const copy = createHistogram();
    copy.add(largeCount);
    largeCount.add(copy);
  }
  largeCount.record(1);
  const largeCountResult =
    await largeCount.qrde({ bins: 2, dequantize: 'none' });
  assert.strictEqual(largeCountResult.count, (1n << 53n) + 1n);
  assertClose(largeCountResult.quantiles[1], 2);

  // Exercise correction across the exact-to-asymptotic beta CDF threshold.
  const correctionOptions = { highest: 131071, figures: 5 };
  const correction = createHistogram(correctionOptions);
  recordRepeated(correction, correctionOptions, 1, 26239);
  recordRepeated(correction, correctionOptions, 131071, 973761);
  const count = 1_000_000;
  const threshold = (1 - Math.sqrt(1 - 100_000 / (count + 1))) / 2;
  const corrected = await correction.qrde({
    probabilities: [0, threshold - 1e-10, threshold + 1e-10, 1],
    dequantize: 'none',
  });
  assert.strictEqual(corrected.corrections, 1);
  assert.strictEqual(corrected.quantiles[1], corrected.quantiles[2]);
})().then(common.mustCall());
