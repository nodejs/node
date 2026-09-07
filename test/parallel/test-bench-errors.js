// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { bench, run, suite } = require('node:bench');
const { setTimeout } = require('timers/promises');

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
let lateTimeoutActive = false;
bench('late timeout', { samples: 1, timeout: 5 }, async (b) => {
  lateTimeoutActive = true;
  try {
    b.start();
    await setTimeout(30);
    b.end(1);
  } finally {
    lateTimeoutActive = false;
  }
});
bench('after late timeout', options, common.mustCall((b) => {
  assert.strictEqual(lateTimeoutActive, false);
  completeSample(b);
}));

const signal = AbortSignal.abort(new Error('stop'));
bench('aborted', { samples: 1, signal }, () => {});

bench('duplicate', { samples: 1, params: { value: 1 } }, completeSample);
bench('duplicate', { samples: 1, params: { value: 1 } }, completeSample);
bench('continues', options, completeSample);
bench('timeout', { samples: 1, timeout: 10 }, async () => {
  await new Promise(() => {});
});
const suiteCompletion = suite('after unsettled timeout suite', () => {
  bench('after unsettled timeout', options, common.mustNotCall());
  bench.skip('skipped after unsettled timeout', options, common.mustNotCall());
});
suiteCompletion.then(common.mustCall());

const completions = [];
const sampleNames = [];
let summary;
const stream = run();
stream.on('bench:complete', (result) => completions.push(result));
stream.on('bench:sample', (sample) => sampleNames.push(sample.name));
stream.on('bench:summary', (result) => { summary = result; });
stream.on('end', common.mustCall(() => {
  assert.strictEqual(completions.length, 16);
  assert.deepStrictEqual(summary.counts, {
    __proto__: null,
    completed: 3,
    failed: 12,
    skipped: 1,
    total: 16,
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
  assert.strictEqual(byName.get('after late timeout')[0].error, undefined);
  assert.strictEqual(byName.get('aborted')[0].error.code, 'ABORT_ERR');

  const duplicates = byName.get('duplicate');
  assert.strictEqual(duplicates[0].error, undefined);
  assert.match(duplicates[1].error.message, /duplicate benchmark identity/);
  assert.strictEqual(byName.get('continues')[0].error, undefined);
  const unsettled = byName.get('after unsettled timeout')[0].error;
  assert.strictEqual(unsettled.code, 'ABORT_ERR');
  assert.strictEqual(unsettled.cause.code, 'ERR_OPERATION_FAILED');
  assert.strictEqual(
    byName.get('skipped after unsettled timeout')[0].skip, true);
  setTimeout(40).then(common.mustCall(() => {
    assert.strictEqual(sampleNames.includes('late timeout'), false);
  }));
}));
stream.resume();
