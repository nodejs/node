// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');
const { bench, run } = require('node:bench');
const { json, spec } = require('node:bench/reporters');

bench('json completed', {
  params: { size: 'small' },
  samples: 1,
}, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

bench('json failed', { samples: 1 }, () => {
  const error = new Error('benchmark failed', { cause: 7n });
  error.code = 'ERR_BENCHMARK_EXAMPLE';
  throw error;
});

(async () => {
  const chunks = await run().compose(json).toArray();
  const lines = chunks.join('').trim().split('\n');
  const records = lines.map((line) => JSON.parse(line));

  assert.strictEqual(records.length, 6);
  const sample = records.find(({ type }) => type === 'bench:sample');
  assert.match(sample.data.duration_ns, /^\d+$/);

  const completions = records.filter(
    ({ type }) => type === 'bench:complete');
  const completed = completions.find(
    ({ data }) => data.name === 'json completed').data;
  assert.match(completed.samples[0].duration_ns, /^\d+$/);

  const failed = completions.find(
    ({ data }) => data.name === 'json failed').data;
  assert.strictEqual(failed.error.name, 'Error');
  assert.strictEqual(failed.error.message, 'benchmark failed');
  assert.strictEqual(failed.error.code, 'ERR_BENCHMARK_EXAMPLE');
  assert.strictEqual(failed.error.cause, '7');
  assert.match(failed.error.stack, /benchmark failed/);

  const summary = records.find(
    ({ type }) => type === 'bench:summary').data;
  assert.match(summary.duration_ns, /^\d+$/);
  assert.deepStrictEqual(summary.counts, {
    completed: 1,
    failed: 1,
    skipped: 0,
    total: 2,
  });

  const synthetic = [
    {
      type: 'bench:complete',
      data: {
        name: 'fast',
        params: { size: 'small' },
        samples: [{}, {}],
        summary: {
          mean: 1500,
          median: 1400,
          coefficientOfVariation: 0.1,
          confidenceInterval: { lower: 1000, upper: 2000 },
          skewness: 2,
        },
      },
    },
    {
      type: 'bench:complete',
      data: {
        name: 'later',
        params: {},
        samples: [],
        skip: 'not supported',
      },
    },
    {
      type: 'bench:complete',
      data: {
        name: 'broken',
        params: {},
        samples: [],
        error: new Error('boom'),
      },
    },
    {
      type: 'bench:diagnostic',
      data: { message: 'suite problem' },
    },
    {
      type: 'bench:summary',
      data: {
        counts: { completed: 1, failed: 1, skipped: 1, total: 3 },
      },
    },
  ];
  const specChunks = await Readable.from(synthetic).compose(spec).toArray();
  assert.strictEqual(specChunks.join(''),
                     'benchmark | samples | mean rate | 95% CI | ' +
                     'median rate | warning\n' +
                     'fast [size="small"] | 2 | 1.50k ops/s | ' +
                     '[1.00k ops/s, 2.00k ops/s] | 1.40k ops/s | ' +
                     'noisy, skewed\n' +
                     'later | 0 | - | - | - | skipped: not supported\n' +
                     'broken | 0 | - | - | - | error: boom\n' +
                     'diagnostic: suite problem\n\n' +
                     '1 completed, 1 failed, 1 skipped\n');
})().then(common.mustCall());
