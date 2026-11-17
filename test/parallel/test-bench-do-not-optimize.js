// Flags: --expose-gc --experimental-bench --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');
const { createRunner, doNotOptimize } = require('node:bench');
const { spec } = require('node:bench/reporters');

// The value is returned unchanged.
for (const value of [
  0, -0, 1.5, NaN, Infinity, 1n, 'string', true, null, undefined,
  Symbol('symbol'), {}, [], () => {},
]) {
  assert.ok(Object.is(doNotOptimize(value), value));
}

function result(name, median) {
  return {
    type: 'bench:complete',
    data: {
      name,
      params: {},
      samples: [{}],
      summary: {
        mean: median,
        median,
        coefficientOfVariation: 0,
        confidenceInterval: { lower: median, upper: median },
        skewness: 0,
      },
    },
  };
}

(async () => {
  // Values are released after the sample that produced them.
  const runner = createRunner({ yieldBetweenSamples: false });
  let ref;
  runner.bench('retained', { samples: 1, warmup: 0 }, common.mustCall((b) => {
    const value = {};
    ref = new WeakRef(value);
    b.start();
    doNotOptimize(value);
    b.end();
  }));
  await runner.run().toArray();
  await new Promise(setImmediate);
  globalThis.gc();
  assert.strictEqual(ref.deref(), undefined);

  // The spec reporter flags median rates of at least one operation per
  // nanosecond as implausible.
  const output = (await Readable.from([
    result('plausible', 999_999_999),
    result('implausible', 1e9),
  ]).compose(spec).toArray()).join('');
  assert.match(output, /^plausible \|.* \| $/m);
  assert.match(output, /^implausible \|.* \| implausible$/m);
})().then(common.mustCall());
