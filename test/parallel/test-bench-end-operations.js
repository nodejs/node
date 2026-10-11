// Flags: --experimental-bench --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { createRunner } = require('node:bench');

function waitForTick() {
  const started = process.hrtime.bigint();
  while (process.hrtime.bigint() === started);
}

const runner = createRunner({ yieldBetweenSamples: false });

runner.bench('default operations', { samples: 1, warmup: 0 },
             common.mustCall((b) => {
               b.start();
               waitForTick();
               const sample = b.end();
               assert.strictEqual(sample.operations, 1);
             }));

runner.bench('undefined operations with options', { samples: 1, warmup: 0 },
             common.mustCall((b) => {
               b.start();
               waitForTick();
               const sample = b.end(undefined, { detail: 'value' });
               assert.strictEqual(sample.operations, 1);
               assert.strictEqual(sample.detail, 'value');
             }));

const invalid = [0, -1, 0.5, NaN, Infinity, null, '1'];
for (const operations of invalid) {
  runner.bench(`invalid ${String(operations)}`, { samples: 1, warmup: 0 },
               (b) => {
                 b.start();
                 waitForTick();
                 b.end(operations);
               });
}

runner.bench('record default operations', { samples: 1, warmup: 0 },
             common.mustCall((b) => {
               const sample = b.record({ duration_ns: 10n });
               assert.strictEqual(sample.operations, 1);
               assert.strictEqual(sample.rate, 100_000_000);
             }));

runner.bench('record undefined operations', { samples: 1, warmup: 0 },
             common.mustCall((b) => {
               const sample = b.record({ duration_ns: 10n, operations: undefined });
               assert.strictEqual(sample.operations, 1);
             }));

for (const operations of invalid) {
  runner.bench(`record invalid ${String(operations)}`,
               { samples: 1, warmup: 0 },
               (b) => b.record({ duration_ns: 10n, operations }));
}

(async () => {
  const records = await runner.run().toArray();
  const byName = new Map(records
    .filter(({ type }) => type === 'bench:complete')
    .map(({ data }) => [data.name, data]));

  assert.deepStrictEqual(
    byName.get('default operations').samples.map(({ operations }) => operations),
    [1]);
  assert.deepStrictEqual(
    byName.get('undefined operations with options').samples
      .map(({ operations }) => operations),
    [1]);

  for (const name of ['record default operations',
                      'record undefined operations']) {
    assert.deepStrictEqual(
      byName.get(name).samples.map(({ operations }) => operations), [1]);
  }

  for (const operations of invalid) {
    const code = typeof operations === 'number' ?
      'ERR_OUT_OF_RANGE' : 'ERR_INVALID_ARG_TYPE';
    for (const prefix of ['invalid', 'record invalid']) {
      const { error, samples } =
        byName.get(`${prefix} ${String(operations)}`);
      assert.strictEqual(error.code, code);
      assert.strictEqual(samples.length, 0);
    }
  }
})().then(common.mustCall());
