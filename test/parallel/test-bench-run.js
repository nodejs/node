// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { setImmediate } = require('timers/promises');
const {
  after,
  afterEach,
  before,
  beforeEach,
  bench,
  run,
  suite,
} = require('node:bench');

const calls = [];
const contexts = new Set();
let active = false;

before(() => calls.push('root before'));
after(() => calls.push('root after'));
beforeEach(() => calls.push('root beforeEach'));
afterEach(() => calls.push('root afterEach'));

const suiteCompletion = suite('group', { tags: ['Group'] }, async () => {
  await setImmediate();

  before(() => calls.push('suite before'));
  after(() => calls.push('suite after'));
  beforeEach(() => calls.push('suite beforeEach'));
  afterEach(() => calls.push('suite afterEach'));

  bench('sync', {
    params: { z: 2, a: true },
    samples: 2,
    tags: ['SYNC'],
    warmup: 1,
  }, common.mustCall((b) => {
    assert.strictEqual(active, false);
    active = true;
    contexts.add(b);
    calls.push('sync sample');
    assert.deepStrictEqual(b.params, { __proto__: null, a: true, z: 2 });
    completeSample(b);
    active = false;
  }, 3));

  bench('async', { samples: 2 }, common.mustCall(async (b) => {
    assert.strictEqual(active, false);
    active = true;
    contexts.add(b);
    calls.push('async sample');
    await setImmediate();
    completeSample(b);
    await setImmediate();
    active = false;
  }, 2));

  bench.skip('skipped', { samples: 1 }, common.mustNotCall());
});

const records = [];
const plans = [];
const stream = run();
stream.on('bench:plan', common.mustCall((plan) => {
  assert.deepStrictEqual(calls, []);
  plans.push(plan.name);
}, 3));
stream.on('data', (record) => records.push(record));
stream.on('end', common.mustCall(() => {
  assert.strictEqual(active, false);
  assert.strictEqual(contexts.size, 5);

  const starts = records.filter(({ type }) => type === 'bench:start');
  const samples = records.filter(({ type }) => type === 'bench:sample');
  const completions = records.filter(({ type }) => type === 'bench:complete');
  const summaries = records.filter(({ type }) => type === 'bench:summary');

  assert.strictEqual(starts.length, 2);
  assert.strictEqual(samples.length, 4);
  assert.strictEqual(completions.length, 3);
  assert.strictEqual(summaries.length, 1);
  assert.deepStrictEqual(plans, ['sync', 'async', 'skipped']);

  const sync = completions.find(({ data }) => data.name === 'sync').data;
  assert.strictEqual(sync.error, undefined);
  assert.strictEqual(sync.skip, undefined);
  assert.strictEqual(sync.samples.length, 2);
  assert.strictEqual(Object.getPrototypeOf(sync), null);
  assert.strictEqual(Object.getPrototypeOf(sync.params), null);
  assert.deepStrictEqual(sync.tags, ['group', 'sync']);
  assert.match(sync.benchId, /\{"a":true,"z":2\}/);
  assert.notStrictEqual(sync.parentId, null);
  assert.strictEqual(sync.summary.mean > 0, true);
  assert.strictEqual(sync.summary.min <= sync.summary.mean, true);
  assert.strictEqual(sync.summary.mean <= sync.summary.max, true);
  assert.strictEqual(typeof sync.summary.confidenceInterval.lower, 'number');
  assert.strictEqual(typeof sync.samples[0].duration_ns, 'bigint');

  const asyncResult = completions.find(
    ({ data }) => data.name === 'async').data;
  assert.strictEqual(asyncResult.error, undefined);
  assert.strictEqual(asyncResult.skip, undefined);
  assert.strictEqual(asyncResult.samples.length, 2);

  const skipped = completions.find(
    ({ data }) => data.name === 'skipped').data;
  assert.strictEqual(skipped.skip, true);
  assert.deepStrictEqual(skipped.samples, []);

  assert.deepStrictEqual(summaries[0].data.counts, {
    __proto__: null,
    completed: 2,
    failed: 0,
    skipped: 1,
    total: 3,
  });
  assert.strictEqual(summaries[0].data.success, true);

  assert.deepStrictEqual(calls, [
    'root before',
    'suite before',
    'root beforeEach',
    'suite beforeEach',
    'sync sample',
    'sync sample',
    'sync sample',
    'suite afterEach',
    'root afterEach',
    'root beforeEach',
    'suite beforeEach',
    'async sample',
    'async sample',
    'suite afterEach',
    'root afterEach',
    'suite after',
    'root after',
  ]);
}));

suiteCompletion.then(common.mustCall());
