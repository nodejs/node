// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
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
    const sample = completeSample(b, 2, { detail });
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

  const variableSamples = [
    { __proto__: null, duration_ns: 1_000_000_000n, operations: 1 },
    { __proto__: null, duration_ns: 100_000_000n, operations: 100 },
  ];
  const variableCompletion = runner.bench('variable batch', {
    samples: variableSamples.length,
  }, common.mustCall((b) => {
    b.record(variableSamples[b.index]);
  }, variableSamples.length));

  const records = await runner.run().toArray();
  const [controlled, recorded, variable] = await Promise.all([
    controlledCompletion,
    recordedCompletion,
    variableCompletion,
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
  assert.deepStrictEqual(variable.samples.map(({ rate }) => rate), [1, 1000]);
  assert.strictEqual(variable.summary.mean, 500.5);
  const pooledRate = 1_000_000_000 * 101 / 1_100_000_000;
  assert.notStrictEqual(variable.summary.mean, pooledRate);
  assert.strictEqual(
    records.filter(({ type }) => type === 'bench:sample').length, 5);
  assert.throws(() => closedContext.start(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => closedContext.end(1), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => closedContext.record({
    duration_ns: 1n,
    operations: 1,
  }), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => closedContext.done(), { code: 'ERR_INVALID_STATE' });
})().then(common.mustCall());
