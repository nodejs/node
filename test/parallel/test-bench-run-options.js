// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { bench, run } = require('node:bench');

let invocations = 0;
const timeout = common.platformTimeout(1000);
bench('overridden', { samples: 8, timeout, warmup: 8 }, (b) => {
  invocations++;
  completeSample(b, invocations);
});

const plans = [];
const samples = [];
const types = [];
const stream = run({
  samples: 2,
  warmup: 3,
  yieldBetweenSamples: false,
});
stream.on('bench:plan', (plan) => plans.push(plan));
stream.on('bench:sample', ({ operations }) => samples.push(operations));
stream.on('data', ({ type }) => types.push(type));
stream.on('end', common.mustCall(() => {
  assert.strictEqual(plans.length, 1);
  assert.deepStrictEqual({
    samples: plans[0].samples,
    selected: plans[0].selected,
    skip: plans[0].skip,
    timeout: plans[0].timeout,
    warmup: plans[0].warmup,
    yieldBetweenSamples: plans[0].yieldBetweenSamples,
  }, {
    samples: 2,
    selected: true,
    skip: undefined,
    timeout,
    warmup: 3,
    yieldBetweenSamples: false,
  });
  assert.deepStrictEqual(samples, [4, 5]);
  assert.deepStrictEqual(types.slice(0, 2), ['bench:plan', 'bench:start']);
}));
stream.resume();
