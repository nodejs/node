// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { createRunner } = require('node:bench');
const { setImmediate } = require('timers/promises');

async function testSynchronousSuiteFailure() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const completion = runner.suite('outer', () => {
    runner.suite('nested', () => {
      runner.bench('blocked', { samples: 1 }, common.mustNotCall());
    });
    throw new Error('synchronous suite failure');
  });
  const records = await runner.run().toArray();
  await completion;
  const plan = records.find(
    ({ type }) => type === 'bench:plan').data;
  const result = records.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(plan.name, 'blocked');
  assert.strictEqual(plan.selected, true);
  assert.strictEqual(
    records.some(({ type }) => type === 'bench:start'), false);
  assert.strictEqual(result.name, 'blocked');
  assert.strictEqual(result.error.message, 'synchronous suite failure');
}

async function testRunSignal() {
  const runner = createRunner();
  const controller = new AbortController();
  let invocations = 0;
  let abortPromise;
  const completion = runner.bench('aborted between samples', {
    samples: 3,
  }, (b) => {
    invocations++;
    completeSample(b);
    if (b.index === 0) {
      abortPromise = setImmediate().then(() => {
        controller.abort(new Error('run aborted'));
      });
    }
  });
  await runner.run({ signal: controller.signal }).toArray();
  const result = await completion;
  await abortPromise;
  await setImmediate();
  assert.strictEqual(result.error.code, 'ABORT_ERR');
  assert.strictEqual(result.error.cause.message, 'run aborted');
  assert.strictEqual(invocations, 1);
}

async function testRunSignalAfterSample() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const controller = new AbortController();
  const reason = new Error('sample aborted');
  const completion = runner.bench('aborted after sample', {
    samples: 2,
  }, completeSample);
  const stream = runner.run({ signal: controller.signal });
  stream.once('bench:sample', common.mustCall(() => {
    controller.abort(reason);
  }));
  const records = await stream.toArray();
  const result = await completion;
  assert.strictEqual(result.error.code, 'ABORT_ERR');
  assert.strictEqual(result.error.cause, reason);
  assert.strictEqual(result.samples.length, 1);
  assert.strictEqual(records.at(-1).data.success, false);
}

async function testStringNamePattern() {
  const runner = createRunner({ yieldBetweenSamples: false });
  runner.bench('included', { samples: 1 }, completeSample);
  runner.bench('excluded', { samples: 1 }, common.mustNotCall());
  const records = await runner.run({ namePattern: 'included' }).toArray();
  const excluded = records.find(
    ({ type, data }) => type === 'bench:complete' &&
      data.name === 'excluded').data;
  const included = records.find(
    ({ type, data }) => type === 'bench:complete' &&
      data.name === 'included').data;
  assert.strictEqual(included.error, undefined);
  assert.strictEqual(included.samples.length, 1);
  assert.strictEqual(excluded.skip, 'name pattern');
}

async function testTopLevelRecovery() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const suiteCompletion = runner.suite('nested', () => {
    runner.bench('listener failure', { samples: 1 }, completeSample);
  });
  const stream = runner.run();
  const failure = new Error();
  failure.message = undefined;
  stream.on('bench:start', common.mustCall(() => { throw failure; }));
  const records = await stream.toArray();
  await suiteCompletion;
  const diagnostic = records.find(
    ({ type }) => type === 'bench:diagnostic').data;
  const summary = records.find(({ type }) => type === 'bench:summary').data;
  assert.strictEqual(diagnostic.message, 'Error');
  assert.strictEqual(diagnostic.file, undefined);
  assert.strictEqual(diagnostic.line, undefined);
  assert.strictEqual(diagnostic.column, undefined);
  assert.strictEqual(summary.duration_ns, 0n);
  assert.strictEqual(summary.success, false);
}

async function testRepeatedReportingFailure() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const original = new Error('start listener failed');
  const diagnostic = new Error('diagnostic listener failed');
  const summary = new Error('summary listener failed');
  const completion = runner.bench('reporting failures', {
    samples: 1,
  }, completeSample);
  const stream = runner.run();
  stream.on('bench:start', common.mustCall(() => { throw original; }));
  stream.on('bench:diagnostic', common.mustCall(() => {
    throw diagnostic;
  }, 2));
  stream.on('bench:summary', common.mustCall(() => { throw summary; }));
  const ended = new Promise((resolve) => stream.once('end', resolve));
  stream.resume();
  await ended;
  assert.strictEqual((await completion).error, original);
}

(async () => {
  await testSynchronousSuiteFailure();
  await testRunSignal();
  await testRunSignalAfterSample();
  await testStringNamePattern();
  await testTopLevelRecovery();
  await testRepeatedReportingFailure();
})().then(common.mustCall());
