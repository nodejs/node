// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { createRunner } = require('node:bench');
const { setImmediate } = require('timers/promises');

(async () => {
  const first = createRunner({ yieldBetweenSamples: false });
  const second = createRunner({ yieldBetweenSamples: false });
  let firstCalls = 0;
  let secondCalls = 0;

  first.before(common.mustCall());
  second.before(common.mustCall());

  const firstCompletion = first.bench(
    'same name', { samples: 2 }, common.mustCall((b) => {
      firstCalls++;
      completeSample(b);
    }, 2));
  const secondCompletion = second.bench(
    'same name', { samples: 1 }, common.mustCall((b) => {
      secondCalls++;
      completeSample(b);
    }));

  await setImmediate();
  assert.strictEqual(firstCalls, 0);
  assert.strictEqual(secondCalls, 0);

  const firstStream = first.run();
  const secondStream = second.run();
  assert.throws(() => first.run(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => first.bench('late', common.mustNotCall()),
                { code: 'ERR_INVALID_STATE' });
  const [firstRecords, secondRecords] = await Promise.all([
    firstStream.toArray(),
    secondStream.toArray(),
  ]);
  const [firstResult, secondResult] = await Promise.all([
    firstCompletion,
    secondCompletion,
  ]);

  assert.strictEqual(firstCalls, 2);
  assert.strictEqual(secondCalls, 1);
  assert.strictEqual(firstResult.samples.length, 2);
  assert.strictEqual(secondResult.samples.length, 1);
  assert.strictEqual(firstResult.error, undefined);
  assert.strictEqual(secondResult.error, undefined);
  assert.strictEqual(firstResult.benchId, secondResult.benchId);
  assert.notStrictEqual(firstResult.runId, secondResult.runId);
  assert.strictEqual(firstResult.fileRunId, firstResult.runId);
  assert.strictEqual(secondResult.fileRunId, secondResult.runId);
  assert.strictEqual(firstResult.entryFile, process.argv[1]);
  assert.strictEqual(secondResult.entryFile, process.argv[1]);
  assert.deepStrictEqual(firstResult.namePath, ['same name']);
  assert.deepStrictEqual(secondResult.namePath, ['same name']);
  assert(firstRecords.every(({ data }) => data.runId === firstResult.runId));
  assert(secondRecords.every(({ data }) => data.runId === secondResult.runId));
  assert.strictEqual(
    firstRecords.filter(({ type }) => type === 'bench:summary').length, 1);
  assert.strictEqual(
    secondRecords.filter(({ type }) => type === 'bench:summary').length, 1);
  assert.strictEqual(typeof first.bench.skip, 'function');
  assert.strictEqual(typeof first.bench.only, 'function');
  assert.strictEqual(first.describe, first.suite);

  const retry = createRunner({ yieldBetweenSamples: false });
  retry.bench('not filtered', { samples: 1 }, (b) => {
    completeSample(b);
  });
  assert.throws(() => retry.run({
    namePattern: 'filtered',
    samples: 0,
  }), { code: 'ERR_OUT_OF_RANGE' });
  const retryRecords = await retry.run().toArray();
  const retryResult = retryRecords.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(retryResult.skip, undefined);
  assert.strictEqual(retryResult.samples.length, 1);

  const reentrant = createRunner({ yieldBetweenSamples: false });
  reentrant.bench('reentrant options', { samples: 1 }, (b) => {
    completeSample(b);
  });
  const reentrantOptions = {};
  Object.defineProperty(reentrantOptions, 'samples', {
    get: common.mustCall(() => reentrant.run()),
  });
  assert.throws(() => reentrant.run(reentrantOptions),
                { code: 'ERR_INVALID_STATE' });
  const reentrantRecords = await reentrant.run().toArray();
  const reentrantResult = reentrantRecords.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(reentrantResult.samples.length, 1);
})().then(common.mustCall());
