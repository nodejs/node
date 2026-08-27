// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { bench, run } = require('node:bench');

const options = { samples: 1 };

bench('missing start', options, () => {});
bench('missing end', options, (b) => b.start());
bench('end before start', options, (b) => b.end(1));
bench('duplicate start', options, (b) => {
  b.start();
  b.start();
});
bench('duplicate end', options, (b) => {
  b.start();
  b.end(1);
  b.end(1);
});
bench('invalid operations', options, (b) => {
  b.start();
  b.end(0);
});
bench('throws', options, () => {
  throw new Error('benchmark failure');
});
bench('timeout', { samples: 1, timeout: 10 }, async () => {
  await new Promise(() => {});
});
bench('late timeout', { samples: 1, timeout: 5 }, async (b) => {
  b.start();
  await new Promise((resolve) => setTimeout(resolve, 30));
  b.end(1);
});

const signal = AbortSignal.abort(new Error('stop'));
bench('aborted', { samples: 1, signal }, () => {});

function complete(b) {
  b.start();
  process.hrtime.bigint();
  b.end(1);
}

bench('duplicate', { samples: 1, params: { value: 1 } }, complete);
bench('duplicate', { samples: 1, params: { value: 1 } }, complete);
bench('continues', options, complete);

const completions = [];
const sampleNames = [];
let summary;
const stream = run();
stream.on('bench:complete', (result) => completions.push(result));
stream.on('bench:sample', (sample) => sampleNames.push(sample.name));
stream.on('bench:summary', (result) => { summary = result; });
stream.on('end', common.mustCall(() => {
  assert.strictEqual(completions.length, 13);
  assert.deepStrictEqual(summary.counts, {
    __proto__: null,
    completed: 2,
    failed: 11,
    skipped: 0,
    total: 13,
  });
  assert.strictEqual(summary.success, false);

  const byName = new Map();
  for (const result of completions) {
    const values = byName.get(result.name) ?? [];
    values.push(result);
    byName.set(result.name, values);
  }

  assert.match(byName.get('missing start')[0].error.message,
               /did not call start/);
  assert.match(byName.get('missing end')[0].error.message,
               /did not call end/);
  assert.match(byName.get('end before start')[0].error.message,
               /before start/);
  assert.strictEqual(byName.get('duplicate start')[0].error.code,
                     'ERR_INVALID_STATE');
  assert.strictEqual(byName.get('duplicate end')[0].error.code,
                     'ERR_INVALID_STATE');
  assert.strictEqual(byName.get('invalid operations')[0].error.code,
                     'ERR_OUT_OF_RANGE');
  assert.strictEqual(byName.get('throws')[0].error.message,
                     'benchmark failure');
  assert.strictEqual(byName.get('timeout')[0].error.code,
                     'ERR_OPERATION_FAILED');
  assert.strictEqual(byName.get('late timeout')[0].error.code,
                     'ERR_OPERATION_FAILED');
  assert.strictEqual(byName.get('aborted')[0].error.code, 'ABORT_ERR');

  const duplicates = byName.get('duplicate');
  assert.strictEqual(duplicates[0].error, undefined);
  assert.match(duplicates[1].error.message, /duplicate benchmark identity/);
  assert.strictEqual(byName.get('continues')[0].error, undefined);
  setTimeout(common.mustCall(() => {
    assert.strictEqual(sampleNames.includes('late timeout'), false);
  }), 40);
}));
stream.resume();
