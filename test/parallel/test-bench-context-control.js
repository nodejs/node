// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { createRunner } = require('node:bench');

(async () => {
  const runner = createRunner({ yieldBetweenSamples: false });
  const invocations = [];
  let closedContext;

  const controlledCompletion = runner.bench('controlled', {
    samples: 5,
    warmup: 2,
  }, common.mustCall((b) => {
    invocations.push(`${b.phase}:${b.index}`);
    const detail = { index: b.index, phase: b.phase };
    b.start();
    process.hrtime.bigint();
    const sample = b.end(2, { detail });
    detail.index = -1;

    assert.strictEqual(sample.operations, 2);
    assert.strictEqual(typeof sample.duration_ns, 'bigint');
    assert.strictEqual(sample.rate > 0, true);
    assert.notStrictEqual(sample.detail, detail);
    assert.notStrictEqual(sample.detail.index, -1);
    sample.operations = 0;
    sample.rate = NaN;
    sample.detail.index = -2;

    if (b.phase === 'measurement' && b.index === 1) {
      b.done();
      closedContext = b;
    }
  }, 4));

  const recordedCompletion = runner.bench(
    'recorded', { samples: 3 }, common.mustCall((b) => {
      const detail = { source: 'worker', value: 1n };
      const sample = b.record({
        __proto__: null,
        detail,
        duration_ns: 20n,
        operations: 5,
      });
      detail.source = 'changed';
      assert.deepStrictEqual(sample, {
        __proto__: null,
        detail: { source: 'worker', value: 1n },
        duration_ns: 20n,
        operations: 5,
        rate: 250_000_000,
      });
      sample.operations = 0;
      sample.rate = NaN;
      sample.detail.source = 'returned value changed';
      b.done();
    }));

  const records = await runner.run().toArray();
  const [controlled, recorded] = await Promise.all([
    controlledCompletion,
    recordedCompletion,
  ]);

  assert.deepStrictEqual(invocations, [
    'warmup:0',
    'warmup:1',
    'measurement:0',
    'measurement:1',
  ]);
  assert.strictEqual(controlled.samples.length, 2);
  assert.deepStrictEqual(
    controlled.samples.map(({ operations }) => operations), [2, 2]);
  assert.deepStrictEqual(
    controlled.samples.map(({ detail }) => detail.index), [0, 1]);
  assert.strictEqual(controlled.samples.every(({ rate }) => rate > 0), true);
  assert.strictEqual(recorded.samples.length, 1);
  assert.deepStrictEqual(recorded.samples[0].detail,
                         { source: 'worker', value: 1n });
  assert.strictEqual(
    records.filter(({ type }) => type === 'bench:sample').length, 3);
  assert.throws(() => closedContext.start(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => closedContext.end(1), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => closedContext.record({
    duration_ns: 1n,
    operations: 1,
  }), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => closedContext.done(), { code: 'ERR_INVALID_STATE' });
})().then(common.mustCall());
