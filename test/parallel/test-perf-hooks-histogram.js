'use strict';

const common = require('../common');

const assert = require('assert');

const {
  createHistogram,
  monitorEventLoopDelay,
} = require('perf_hooks');

const { inspect } = require('util');

{
  const h = createHistogram();

  assert.strictEqual(h.min, 9223372036854776000);
  assert.strictEqual(h.minBigInt, 9223372036854775807n);
  assert.strictEqual(h.max, 0);
  assert.strictEqual(h.maxBigInt, 0n);
  assert.strictEqual(h.exceeds, 0);
  assert.strictEqual(h.exceedsBigInt, 0n);
  assert.ok(Number.isNaN(h.mean));
  assert.ok(Number.isNaN(h.stddev));

  assert.strictEqual(h.count, 0);
  assert.strictEqual(h.countBigInt, 0n);

  h.record(1);

  assert.strictEqual(h.count, 1);
  assert.strictEqual(h.countBigInt, 1n);

  [false, '', {}, undefined, null].forEach((i) => {
    assert.throws(() => h.record(i), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });
  [0, Number.MAX_SAFE_INTEGER + 1].forEach((i) => {
    assert.throws(() => h.record(i), {
      code: 'ERR_OUT_OF_RANGE'
    });
  });

  assert.strictEqual(h.min, 1);
  assert.strictEqual(h.minBigInt, 1n);
  assert.strictEqual(h.max, 1);
  assert.strictEqual(h.maxBigInt, 1n);
  assert.strictEqual(h.exceeds, 0);
  assert.strictEqual(h.mean, 1);
  assert.strictEqual(h.stddev, 0);

  assert.strictEqual(h.percentile(1), 1);
  assert.strictEqual(h.percentile(100), 1);

  assert.strictEqual(h.percentileBigInt(1), 1n);
  assert.strictEqual(h.percentileBigInt(100), 1n);

  assert.deepStrictEqual(h.percentiles, new Map([[0, 1], [100, 1]]));

  assert.deepStrictEqual(h.percentilesBigInt, new Map([[0, 1n], [100, 1n]]));

  const mc = new MessageChannel();
  mc.port1.onmessage = common.mustCall(({ data }) => {
    assert.strictEqual(h.min, 1);
    assert.strictEqual(h.max, 1);
    assert.strictEqual(h.exceeds, 0);
    assert.strictEqual(h.mean, 1);
    assert.strictEqual(h.stddev, 0);

    data.record(2n);
    data.recordDelta();

    assert.strictEqual(h.max, 2);

    mc.port1.close();
  });
  mc.port2.postMessage(h);
}

{
  const e = monitorEventLoopDelay();
  assert.strictEqual(e.count, 0);
  e.enable();
  const mc = new MessageChannel();
  mc.port1.onmessage = common.mustCall(({ data }) => {
    assert.strictEqual(typeof data.min, 'number');
    assert.ok(data.min > 0);
    assert.ok(data.count > 0);
    assert.strictEqual(data.disable, undefined);
    assert.strictEqual(data.enable, undefined);
    mc.port1.close();
  });
  const interval = setInterval(() => {
    if (e.count > 0) {
      clearInterval(interval);
      mc.port2.postMessage(e);
    }
  }, 50);
}

{
  // Tests that the ELD histogram is disposable
  let histogram;
  {
    using hi = monitorEventLoopDelay();
    histogram = hi;
  }
  // The histogram should already be disabled.
  assert.strictEqual(histogram.disable(), false);
}

{
  const h = createHistogram();
  assert.ok(inspect(h, { depth: null }).startsWith('Histogram'));
  assert.strictEqual(inspect(h, { depth: -1 }), '[RecordableHistogram]');
}

{
  // Tests that RecordableHistogram is impossible to construct manually
  const h = createHistogram();
  assert.throws(() => new h.constructor(), { code: 'ERR_ILLEGAL_CONSTRUCTOR' });
}

{
  [
    'hello',
    1,
    null,
  ].forEach((i) => {
    assert.throws(() => createHistogram(i), { code: 'ERR_INVALID_ARG_TYPE' });
  });

  [
    'hello',
    false,
    null,
    {},
  ].forEach((i) => {
    assert.throws(() => createHistogram({ lowest: i }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
    assert.throws(() => createHistogram({ highest: i }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
    assert.throws(() => createHistogram({ figures: i }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });

  // Number greater than 5 is not allowed
  for (const i of [6, 10]) {
    assert.throws(() => createHistogram({ figures: i }), {
      code: 'ERR_OUT_OF_RANGE',
    });
  }

  createHistogram({ lowest: 1, highest: 11, figures: 1 });
}

{
  const h1 = createHistogram();
  const h2 = createHistogram();

  h1.record(1);

  assert.strictEqual(h2.count, 0);
  assert.strictEqual(h1.count, 1);

  h2.add(h1);

  assert.strictEqual(h2.count, 1);

  [
    'hello',
    1,
    false,
    {},
  ].forEach((i) => {
    assert.throws(() => h1.add(i), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });
}
