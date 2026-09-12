'use strict';

const common = require('../common');
const assert = require('assert');
const { setTimeout: delay } = require('timers/promises');
const { MessageChannel } = require('worker_threads');
const {
  createSlidingWindowHistogram,
} = require('perf_hooks');

{
  const histogram = createSlidingWindowHistogram({
    chunks: 3,
    recordsPerChunk: 2,
    highest: 100,
  });

  assert.strictEqual(histogram.constructor.name, 'SlidingWindowHistogram');
  assert.strictEqual(histogram.recordDelta, undefined);
  assert.strictEqual(histogram.snapshot().count, 0);

  for (let value = 1; value <= 6; value++) histogram.record(value);

  const full = histogram.snapshot();
  assert.strictEqual(full.count, 6);
  assert.strictEqual(full.min, 1);
  assert.strictEqual(full.max, 6);
  assert.strictEqual(full.record, undefined);

  histogram.record(7);
  let current = histogram.snapshot();
  assert.strictEqual(current.count, 5);
  assert.strictEqual(current.min, 3);
  assert.strictEqual(current.max, 7);

  histogram.record(8);
  histogram.record(9);
  current = histogram.snapshot();
  assert.strictEqual(current.count, 5);
  assert.strictEqual(current.min, 5);
  assert.strictEqual(current.max, 9);

  // Materialized snapshots do not change with the sliding window.
  assert.strictEqual(full.count, 6);
  assert.strictEqual(full.min, 1);
  assert.strictEqual(full.max, 6);

  histogram.reset();
  assert.strictEqual(histogram.snapshot().count, 0);
  histogram.record(10n);
  assert.strictEqual(histogram.snapshot().maxBigInt, 10n);
  for (const value of [0n, 2n ** 63n]) {
    assert.throws(() => histogram.record(value), {
      code: 'ERR_OUT_OF_RANGE',
    });
  }

  assert.throws(() => new histogram.constructor(), {
    code: 'ERR_ILLEGAL_CONSTRUCTOR',
  });
  assert.throws(() => histogram.record.call({}, 1), {
    code: 'ERR_INVALID_THIS',
  });
  assert.throws(() => histogram.snapshot.call({}), {
    code: 'ERR_INVALID_THIS',
  });
  assert.throws(() => histogram.reset.call({}), {
    code: 'ERR_INVALID_THIS',
  });
  assert.throws(() => structuredClone(histogram), {
    name: 'DataCloneError',
  });

  const { port1, port2 } = new MessageChannel();
  assert.throws(() => port1.postMessage(histogram), {
    name: 'DataCloneError',
  });
  assert.throws(() => port1.postMessage(histogram, [histogram]), {
    name: 'DataCloneError',
  });
  port1.close();
  port2.close();
}

{
  const histogram = createSlidingWindowHistogram({
    chunks: 2,
    recordsPerChunk: 1,
    highest: 10,
  });

  // Out-of-range recording attempts count toward count-based rotation.
  histogram.record(11);
  histogram.record(1);
  let current = histogram.snapshot();
  assert.strictEqual(current.count, 1);
  assert.strictEqual(current.exceeds, 1);

  histogram.record(2);
  current = histogram.snapshot();
  assert.strictEqual(current.count, 2);
  assert.strictEqual(current.exceeds, 0);
}

{
  for (const options of [
    undefined,
    null,
    {},
    { chunks: 2 },
    { chunks: 2, chunkDuration: 1, recordsPerChunk: 1 },
  ]) {
    assert.throws(() => createSlidingWindowHistogram(options), {
      code: options?.chunks === undefined ?
        'ERR_INVALID_ARG_TYPE' : 'ERR_INVALID_ARG_VALUE',
    });
  }

  for (const chunks of [0, 1025, 1.5, '2']) {
    assert.throws(() => createSlidingWindowHistogram({
      chunks,
      recordsPerChunk: 1,
    }), {
      code: typeof chunks === 'number' ?
        'ERR_OUT_OF_RANGE' : 'ERR_INVALID_ARG_TYPE',
    });
  }

  for (const chunkDuration of [0, 1.5, 18_446_744_073_710]) {
    assert.throws(() => createSlidingWindowHistogram({
      chunks: 2,
      chunkDuration,
    }), { code: 'ERR_OUT_OF_RANGE' });
  }

  for (const recordsPerChunk of [0, 1.5, Number.MAX_SAFE_INTEGER + 1]) {
    assert.throws(() => createSlidingWindowHistogram({
      chunks: 2,
      recordsPerChunk,
    }), { code: 'ERR_OUT_OF_RANGE' });
  }

  assert.throws(() => createSlidingWindowHistogram({
    chunks: 2,
    recordsPerChunk: 1,
    lowest: 10,
    highest: 10,
  }), { code: 'ERR_OUT_OF_RANGE' });

  for (const [name, value] of [
    ['lowest', 0n],
    ['lowest', 2n ** 63n],
    ['highest', 0n],
    ['highest', 2n ** 63n],
  ]) {
    assert.throws(() => createSlidingWindowHistogram({
      chunks: 2,
      recordsPerChunk: 1,
      [name]: value,
    }), { code: 'ERR_OUT_OF_RANGE' });
  }

  for (const bounds of [
    { lowest: 1n },
    { lowest: 1n, highest: 100 },
    { lowest: 1, highest: 100n },
  ]) {
    const histogram = createSlidingWindowHistogram({
      chunks: 1,
      recordsPerChunk: 1,
      ...bounds,
    });
    histogram.record(1);
    assert.strictEqual(histogram.snapshot().count, 1);
  }
}

(async () => {
  const histogram = createSlidingWindowHistogram({
    chunks: 1,
    chunkDuration: 100,
    highest: 100,
  });

  histogram.record(1);
  assert.strictEqual(histogram.snapshot().count, 1);

  await delay(common.platformTimeout(200));
  assert.strictEqual(histogram.snapshot().count, 0);

  histogram.record(2);
  const current = histogram.snapshot();
  assert.strictEqual(current.count, 1);
  assert.strictEqual(current.min, 2);
})().then(common.mustCall());
