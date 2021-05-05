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
  assert.strictEqual(h.max, 0);
  assert.strictEqual(h.exceeds, 0);
  assert(Number.isNaN(h.mean));
  assert(Number.isNaN(h.stddev));

  h.record(1);

  [false, '', {}, undefined, null].forEach((i) => {
    assert.throws(() => h.record(i), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });
  assert.throws(() => h.record(0, Number.MAX_SAFE_INTEGER + 1), {
    code: 'ERR_OUT_OF_RANGE'
  });

  assert.strictEqual(h.min, 1);
  assert.strictEqual(h.max, 1);
  assert.strictEqual(h.exceeds, 0);
  assert.strictEqual(h.mean, 1);
  assert.strictEqual(h.stddev, 0);

  assert.strictEqual(h.percentile(1), 1);
  assert.strictEqual(h.percentile(100), 1);

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
  e.enable();
  const mc = new MessageChannel();
  mc.port1.onmessage = common.mustCall(({ data }) => {
    assert(typeof data.min, 'number');
    assert(data.min > 0);
    assert.strictEqual(data.disable, undefined);
    assert.strictEqual(data.enable, undefined);
    mc.port1.close();
  });
  setTimeout(() => mc.port2.postMessage(e), 100);
}

{
  const h = createHistogram();
  assert(inspect(h, { depth: null }).startsWith('Histogram'));
  assert.strictEqual(inspect(h, { depth: -1 }), '[RecordableHistogram]');
}

{
  // Tests that RecordableHistogram is impossible to construct manually
  const h = createHistogram();
  assert.throws(
    () => new h.constructor(),
    /^TypeError: illegal constructor$/
  );
}
