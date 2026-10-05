// Flags: --experimental-bench --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { createRunner } = require('node:bench');

// Benchmarks that do not set `warmup` run ten unreported invocations before
// the measured samples.
(async () => {
  const runner = createRunner({ yieldBetweenSamples: false });
  const phases = [];

  const completion = runner.bench('default warmup', {
    samples: 2,
  }, common.mustCall((b) => {
    phases.push(b.phase);
    completeSample(b);
  }, 12));

  const records = await runner.run().toArray();
  const result = await completion;

  const plan = records.find(({ type }) => type === 'bench:plan').data;
  assert.strictEqual(plan.warmup, 10);
  assert.deepStrictEqual(phases, [
    ...Array(10).fill('warmup'),
    'measurement',
    'measurement',
  ]);
  assert.strictEqual(result.samples.length, 2);
  assert.strictEqual(
    records.filter(({ type }) => type === 'bench:sample').length, 2);
})().then(common.mustCall());
