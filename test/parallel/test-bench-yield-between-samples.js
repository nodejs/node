// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { createRunner } = require('node:bench');
const { setImmediate } = require('timers/promises');

async function observe(factoryOptions, runOptions) {
  const runner = createRunner(factoryOptions);
  const observed = [];
  let turnOccurred = false;
  const turn = setImmediate().then(() => {
    turnOccurred = true;
  });

  runner.bench('yielding', { samples: 2 }, (b) => {
    observed.push(turnOccurred);
    completeSample(b);
  });

  await runner.run(runOptions).toArray();
  await turn;
  return observed;
}

async function observeTimeout() {
  const runner = createRunner({ yieldBetweenSamples: false });
  let invocations = 0;
  const completion = runner.bench('timeout', {
    samples: 10,
    timeout: 5,
  }, (b) => {
    invocations++;
    const until = process.hrtime.bigint() + 2_000_000n;
    b.start();
    while (process.hrtime.bigint() < until) { /* Busy loop. */ }
    b.end(1);
  });
  await runner.run().toArray();
  const result = await completion;
  assert.strictEqual(result.error.code, 'ERR_OPERATION_FAILED');
  assert.strictEqual(invocations < 10, true);
}

async function observeAfterEachTimeout() {
  const runner = createRunner({ yieldBetweenSamples: false });
  runner.afterEach(common.mustCall(() => {
    const until = process.hrtime.bigint() + 10_000_000n;
    while (process.hrtime.bigint() < until) { /* Busy loop. */ }
  }));
  const completion = runner.bench('afterEach timeout', {
    samples: 1,
    timeout: 5,
  }, (b) => {
    completeSample(b);
  });
  await runner.run().toArray();
  const result = await completion;
  assert.strictEqual(result.error.code, 'ERR_OPERATION_FAILED');
}

(async () => {
  assert.deepStrictEqual(await observe(undefined, undefined), [false, true]);
  assert.deepStrictEqual(
    await observe({ yieldBetweenSamples: false }, undefined), [false, false]);
  assert.deepStrictEqual(await observe(
    { yieldBetweenSamples: false },
    { yieldBetweenSamples: true }), [false, true]);
  assert.deepStrictEqual(await observe(
    { yieldBetweenSamples: true },
    { yieldBetweenSamples: false }), [false, false]);
  await observeTimeout();
  await observeAfterEachTimeout();
})().then(common.mustCall());
