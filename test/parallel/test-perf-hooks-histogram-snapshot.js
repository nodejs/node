'use strict';

const common = require('../common');
const assert = require('assert');
const { setTimeout: delay } = require('timers/promises');
const { inspect } = require('util');
const {
  createHistogram,
  monitorEventLoopDelay,
} = require('perf_hooks');

function assertSameState(actual, expected) {
  assert.strictEqual(actual.countBigInt, expected.countBigInt);
  assert.strictEqual(actual.minBigInt, expected.minBigInt);
  assert.strictEqual(actual.maxBigInt, expected.maxBigInt);
  assert.strictEqual(actual.exceedsBigInt, expected.exceedsBigInt);
  assert.strictEqual(actual.mean, expected.mean);
  assert.strictEqual(actual.stddev, expected.stddev);
  assert.strictEqual(actual.ewmaMean, expected.ewmaMean);
  assert.strictEqual(actual.ewmaStddev, expected.ewmaStddev);
  assert.strictEqual(actual.ewmaErrorRate, expected.ewmaErrorRate);
  assert.deepStrictEqual(actual.percentilesBigInt, expected.percentilesBigInt);
  // The export format also covers the layout, the raw min and max values,
  // every bucket count, and the EWMA configuration.
  assert.deepStrictEqual(actual.export(), expected.export());
}

function assertReadOnly(snapshot) {
  assert.strictEqual(snapshot.constructor.name, 'Histogram');
  assert.strictEqual(inspect(snapshot, { depth: -1 }), '[Histogram]');
  for (const name of [
    'record',
    'recordDelta',
    'recordCorrected',
    'add',
    'subtract',
    'enable',
    'disable',
  ]) {
    assert.strictEqual(snapshot[name], undefined);
  }
}

{
  const histogram = createHistogram({
    lowest: 1,
    highest: 1000,
    figures: 2,
    halfLife: 8,
    threshold: 50,
  });
  for (let i = 1; i <= 100; i++) histogram.record(i * 7);
  histogram.record(1e6);
  assert.strictEqual(histogram.exceeds, 1);
  assert.ok(histogram.ewmaErrorRate > 0);

  const snapshot = histogram.snapshot();
  assert.notStrictEqual(snapshot, histogram);
  assertReadOnly(snapshot);
  assertSameState(snapshot, histogram);
  assert.strictEqual(histogram.ksTest(snapshot), 0);

  // A snapshot can be snapshotted and cloned.
  const nested = snapshot.snapshot();
  assertReadOnly(nested);
  assertSameState(nested, snapshot);
  assertSameState(structuredClone(snapshot), snapshot);

  // Changes to the source histogram do not change the snapshot.
  const exported = snapshot.export();
  histogram.record(3);
  histogram.recordCorrected(900, 100);
  histogram.record(1e6);
  assert.deepStrictEqual(snapshot.export(), exported);
  assert.strictEqual(snapshot.count, 100);
  assert.strictEqual(snapshot.exceeds, 1);

  histogram.reset();
  assert.strictEqual(histogram.count, 0);
  assert.deepStrictEqual(snapshot.export(), exported);

  // Resetting a snapshot does not change the source histogram or other
  // snapshots.
  histogram.record(5);
  const current = histogram.snapshot();
  snapshot.reset();
  assert.strictEqual(snapshot.count, 0);
  assert.strictEqual(histogram.count, 1);
  assert.strictEqual(current.count, 1);
  assert.deepStrictEqual(nested.export(), exported);
}

{
  // Empty histograms, with and without EWMA state.
  for (const options of [undefined, { halfLife: 10, threshold: 1 }]) {
    const histogram = createHistogram(options);
    const snapshot = histogram.snapshot();
    assertReadOnly(snapshot);
    assert.strictEqual(snapshot.count, 0);
    assert.strictEqual(snapshot.minBigInt, 9223372036854775807n);
    assert.strictEqual(snapshot.maxBigInt, 0n);
    assertSameState(snapshot, histogram);
  }
}

{
  const histogram = createHistogram();
  assert.throws(() => histogram.snapshot.call({}), {
    code: 'ERR_INVALID_THIS',
  });
}

async function testEventLoopDelay(options) {
  const histogram = monitorEventLoopDelay(options);
  histogram.enable();
  while (histogram.count < 3) await delay(1);

  // Samples are only recorded while the event loop is running, so none can
  // be added while this code runs synchronously.
  const snapshot = histogram.snapshot();
  assertReadOnly(snapshot);
  assertSameState(snapshot, histogram);

  const exported = snapshot.export();
  const count = histogram.count;
  while (histogram.count === count) await delay(1);
  assert.deepStrictEqual(snapshot.export(), exported);

  histogram.disable();
  histogram.reset();
  assert.strictEqual(histogram.count, 0);
  assert.deepStrictEqual(snapshot.export(), exported);
}

(async () => {
  await testEventLoopDelay({ resolution: 1 });
  await testEventLoopDelay({ samplePerIteration: true });
})().then(common.mustCall());
