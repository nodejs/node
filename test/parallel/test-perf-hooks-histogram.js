'use strict';

const common = require('../common');

const {
  deepStrictEqual,
  ok,
  strictEqual,
  throws,
} = require('assert');

const {
  createHistogram,
  monitorEventLoopDelay,
} = require('perf_hooks');

const { inspect } = require('util');

{
  const h = createHistogram();

  strictEqual(h.min, 9223372036854776000);
  strictEqual(h.minBigInt, 9223372036854775807n);
  strictEqual(h.max, 0);
  strictEqual(h.maxBigInt, 0n);
  strictEqual(h.exceeds, 0);
  strictEqual(h.exceedsBigInt, 0n);
  ok(Number.isNaN(h.mean));
  ok(Number.isNaN(h.stddev));

  strictEqual(h.count, 0);
  strictEqual(h.countBigInt, 0n);

  h.record(1);

  strictEqual(h.count, 1);
  strictEqual(h.countBigInt, 1n);

  [false, '', {}, undefined, null].forEach((i) => {
    throws(() => h.record(i), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });
  throws(() => h.record(0, Number.MAX_SAFE_INTEGER + 1), {
    code: 'ERR_OUT_OF_RANGE'
  });

  strictEqual(h.min, 1);
  strictEqual(h.minBigInt, 1n);
  strictEqual(h.max, 1);
  strictEqual(h.maxBigInt, 1n);
  strictEqual(h.exceeds, 0);
  strictEqual(h.mean, 1);
  strictEqual(h.stddev, 0);

  strictEqual(h.percentile(1), 1);
  strictEqual(h.percentile(100), 1);

  strictEqual(h.percentileBigInt(1), 1n);
  strictEqual(h.percentileBigInt(100), 1n);

  deepStrictEqual(h.percentiles, new Map([[0, 1], [100, 1]]));

  deepStrictEqual(h.percentilesBigInt, new Map([[0, 1n], [100, 1n]]));

  const mc = new MessageChannel();
  mc.port1.onmessage = common.mustCall(({ data }) => {
    strictEqual(h.min, 1);
    strictEqual(h.max, 1);
    strictEqual(h.exceeds, 0);
    strictEqual(h.mean, 1);
    strictEqual(h.stddev, 0);

    data.record(2n);
    data.recordDelta();

    strictEqual(h.max, 2);

    mc.port1.close();
  });
  mc.port2.postMessage(h);
}

{
  const e = monitorEventLoopDelay();
  strictEqual(e.count, 0);
  e.enable();
  const mc = new MessageChannel();
  mc.port1.onmessage = common.mustCall(({ data }) => {
    strictEqual(typeof data.min, 'number');
    ok(data.min > 0);
    ok(data.count > 0);
    strictEqual(data.disable, undefined);
    strictEqual(data.enable, undefined);
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
  strictEqual(histogram.disable(), false);
}

{
  const h = createHistogram();
  ok(inspect(h, { depth: null }).startsWith('Histogram'));
  strictEqual(inspect(h, { depth: -1 }), '[RecordableHistogram]');
}

{
  // Tests that RecordableHistogram is impossible to construct manually
  const h = createHistogram();
  throws(() => new h.constructor(), { code: 'ERR_ILLEGAL_CONSTRUCTOR' });
}

{
  [
    'hello',
    1,
    null,
  ].forEach((i) => {
    throws(() => createHistogram(i), { code: 'ERR_INVALID_ARG_TYPE' });
  });

  [
    'hello',
    false,
    null,
    {},
  ].forEach((i) => {
    throws(() => createHistogram({ lowest: i }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
    throws(() => createHistogram({ highest: i }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
    throws(() => createHistogram({ figures: i }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });

  // Number greater than 5 is not allowed
  for (const i of [6, 10]) {
    throws(() => createHistogram({ figures: i }), {
      code: 'ERR_OUT_OF_RANGE',
    });
  }

  createHistogram({ lowest: 1, highest: 11, figures: 1 });
}

{
  const h1 = createHistogram();
  const h2 = createHistogram();

  h1.record(1);

  strictEqual(h2.count, 0);
  strictEqual(h1.count, 1);

  h2.add(h1);

  strictEqual(h2.count, 1);

  [
    'hello',
    1,
    false,
    {},
  ].forEach((i) => {
    throws(() => h1.add(i), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });
}
