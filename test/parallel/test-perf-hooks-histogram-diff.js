'use strict';

const common = require('../common');
const assert = require('assert');
const { setTimeout: delay } = require('timers/promises');
const {
  createHistogram,
  createSlidingWindowHistogram,
  importHistogram,
  monitorEventLoopDelay,
} = require('perf_hooks');

{
  const histogram = createHistogram({ highest: 1000, halfLife: 4, threshold: 50 });
  for (let i = 1; i <= 10; i++) histogram.record(i);
  histogram.record(2000);
  const previous = histogram.snapshot();
  for (let i = 101; i <= 120; i++) histogram.record(i);
  histogram.record(3000);
  histogram.record(4000);
  const current = histogram.snapshot();
  const exportedPrevious = previous.export();
  const exportedCurrent = current.export();

  const delta = current.diff(previous);
  assert.strictEqual(delta.constructor.name, 'Histogram');
  assert.strictEqual(delta.record, undefined);
  assert.strictEqual(delta.count, 20);
  assert.strictEqual(delta.exceeds, 2);
  assert.strictEqual(delta.min, 101);
  assert.strictEqual(delta.max, 120);
  assert.strictEqual(delta.resetCount, 0);

  // The difference has the same distribution as a histogram of the values
  // recorded between the snapshots, and no EWMA state.
  const expected = createHistogram({ highest: 1000 });
  for (let i = 101; i <= 120; i++) expected.record(i);
  assert.deepStrictEqual(delta.percentiles, expected.percentiles);
  assert.strictEqual(delta.ewmaMean, 0);
  assert.strictEqual(delta.ewmaStddev, 0);
  assert.strictEqual(delta.ewmaErrorRate, 0);

  // Neither histogram is changed.
  assert.deepStrictEqual(previous.export(), exportedPrevious);
  assert.deepStrictEqual(current.export(), exportedCurrent);

  assert.strictEqual(histogram.diff(previous).count, 20);
  assert.strictEqual(histogram.diff(histogram).count, 0);
  assert.strictEqual(current.diff(current).count, 0);

  // Reversed arguments.
  assert.throws(() => previous.diff(current), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  // Consumers with different intervals each keep their own previous snapshot.
  const histogram = createHistogram();
  const consumers = [3, 10].map((interval) => ({
    interval,
    previous: histogram.snapshot(),
    pending: 0,
    total: 0,
  }));
  for (let i = 1; i <= 100; i++) {
    histogram.record(i);
    for (const consumer of consumers) {
      consumer.pending++;
      if (i % consumer.interval !== 0) continue;
      const current = histogram.snapshot();
      const delta = current.diff(consumer.previous);
      assert.strictEqual(delta.count, consumer.pending);
      assert.strictEqual(delta.min, i - consumer.pending + 1);
      assert.strictEqual(delta.max, i);
      consumer.total += delta.count;
      consumer.previous = current;
      consumer.pending = 0;
    }
  }
  assert.strictEqual(consumers[0].total, 99);
  assert.strictEqual(consumers[1].total, 100);
}

{
  const histogram = createHistogram();
  assert.strictEqual(histogram.resetCount, 0);
  histogram.record(1);
  histogram.recordCorrected(100, 10);
  histogram.add(createHistogram());
  assert.strictEqual(histogram.resetCount, 0);

  histogram.reset();
  assert.strictEqual(histogram.resetCount, 1);
  histogram.record(1);
  const previous = histogram.snapshot();
  histogram.reset();
  assert.strictEqual(histogram.resetCount, 2);
  assert.strictEqual(previous.resetCount, 1);

  // A reset is detected even when every count has grown past its previous
  // value since.
  for (let i = 0; i < 10; i++) histogram.record(1);
  assert.throws(() => histogram.diff(previous), {
    code: 'ERR_INVALID_STATE',
  });

  // subtract() also removes values. Subtracting an empty histogram leaves
  // every count unchanged.
  const snapshot = histogram.snapshot();
  assert.strictEqual(snapshot.resetCount, 2);
  histogram.subtract(createHistogram());
  assert.strictEqual(histogram.resetCount, 3);
  assert.throws(() => histogram.diff(snapshot), {
    code: 'ERR_INVALID_STATE',
  });
}

{
  const histogram = createHistogram();
  for (const options of [{ lowest: 2 }, { highest: 1000 }, { figures: 2 }]) {
    assert.throws(() => histogram.diff(createHistogram(options)), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  }

  // Histograms with a different normalizing index offset map values to
  // different indexes.
  const data = createHistogram().export();
  const offset = Buffer.from(data).indexOf(Buffer.from([0x06, 0x00, 0x07, 0x00]));
  assert.notStrictEqual(offset, -1);
  data[offset + 3] = 1;
  assert.throws(() => histogram.diff(importHistogram(data)), {
    code: 'ERR_INVALID_ARG_VALUE',
  });

  assert.throws(() => histogram.diff.call({}, histogram), {
    code: 'ERR_INVALID_THIS',
  });
  const { get } = Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(histogram.snapshot()), 'resetCount');
  assert.throws(() => get.call({}), { code: 'ERR_INVALID_THIS' });
  const window = createSlidingWindowHistogram({ chunks: 1, recordsPerChunk: 1 });
  for (const other of [undefined, null, {}, 1, window]) {
    assert.throws(() => histogram.diff(other), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }
}

(async () => {
  const histogram = monitorEventLoopDelay({ samplePerIteration: true });
  histogram.enable();
  while (histogram.count < 2) await delay(1);
  const previous = histogram.snapshot();
  while (histogram.count < previous.count + 3) await delay(1);

  // Samples are only recorded while the event loop is running.
  const current = histogram.snapshot();
  assert.strictEqual(current.diff(previous).count,
                     current.count - previous.count);

  histogram.disable();
  histogram.reset();
  assert.strictEqual(histogram.resetCount, 1);
  assert.throws(() => histogram.diff(previous), {
    code: 'ERR_INVALID_STATE',
  });
})().then(common.mustCall());
