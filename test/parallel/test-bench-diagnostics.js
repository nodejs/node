// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { createRunner } = require('node:bench');
const { setImmediate } = require('timers/promises');

function recordSample(b) {
  b.record({
    __proto__: null,
    duration_ns: 1n,
    operations: 1,
  });
}

async function testListenerFailure(callbackError) {
  const runner = createRunner({ yieldBetweenSamples: false });
  const listenerError = new Error('diagnostic listener failed');
  const completion = runner.bench('listener failure', {
    samples: 1,
  }, common.mustCall((b) => {
    b.diagnostic('listener failure');
    if (callbackError !== undefined) throw callbackError;
    recordSample(b);
  }));
  const stream = runner.run();
  stream.on('bench:diagnostic', common.mustCall(() => {
    throw listenerError;
  }));
  await stream.toArray();
  const result = await completion;
  assert.strictEqual(result.error, callbackError ?? listenerError);
  assert.strictEqual(result.samples.length, callbackError === undefined ? 1 : 0);
}

async function testAbortDuringDiagnosticDelivery() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const abortReason = new Error('stop diagnostic delivery');
  const controller = new AbortController();
  const completion = runner.bench('abort diagnostics', {
    samples: 1,
  }, common.mustCall((b) => {
    for (let i = 0; i < 32; i++) b.diagnostic(`diagnostic ${i}`);
    recordSample(b);
  }));
  const stream = runner.run({ signal: controller.signal });
  const iterator = stream[Symbol.asyncIterator]();
  const records = [];
  records.push((await iterator.next()).value);
  for (let i = 0; i < 100 &&
       stream.readableLength < stream.readableHighWaterMark; i++) {
    await setImmediate();
  }
  assert.strictEqual(stream.readableLength, stream.readableHighWaterMark);
  controller.abort(abortReason);
  while (true) {
    const { done, value } = await iterator.next();
    if (done) break;
    records.push(value);
  }

  const result = await completion;
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic');
  const completeIndex = records.findIndex(
    ({ type }) => type === 'bench:complete');
  assert.strictEqual(diagnostics.length, 32);
  assert(records.indexOf(diagnostics.at(-1)) < completeIndex);
  assert.strictEqual(records.at(-1).type, 'bench:summary');
  assert.strictEqual(result.error.code, 'ABORT_ERR');
  assert.strictEqual(result.error.cause, abortReason);
  assert.strictEqual(result.samples.length, 1);
}

async function testAfterEachFailurePrecedence() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const callbackError = new Error('callback failed');
  const afterEachError = new Error('afterEach failed');
  runner.afterEach(common.mustCall(() => {
    throw afterEachError;
  }));
  const completion = runner.bench('afterEach precedence', {
    samples: 1,
  }, common.mustCall((b) => {
    b.diagnostic('before failures');
    throw callbackError;
  }));
  await runner.run().toArray();
  assert.strictEqual((await completion).error, afterEachError);
}

(async () => {
  const runner = createRunner({ yieldBetweenSamples: false });
  let finalContext;
  const completed = runner.bench('diagnostics', {
    samples: 2,
    warmup: 1,
  }, common.mustCall((b) => {
    finalContext = b;
    const detail = { index: b.index };
    const level = b.phase === 'warmup' ? 'info' : 'warning';
    assert.strictEqual(b.diagnostic(`${b.phase} ${b.index}`, {
      detail,
      level,
    }), undefined);
    detail.index = -1;
    recordSample(b);
  }, 3));
  const expectedError = new Error('benchmark failed');
  const failed = runner.bench('failed diagnostic', {
    samples: 1,
  }, common.mustCall((b) => {
    b.diagnostic('before failure', {
      detail: { retained: true },
      level: 'warning',
    });
    throw expectedError;
  }));

  const namedDiagnostics = [];
  const stream = runner.run();
  stream.on('bench:diagnostic', (diagnostic) => {
    namedDiagnostics.push(diagnostic);
  });
  const records = await stream.toArray();
  const completedResult = await completed;
  const failedResult = await failed;
  const diagnosticRecords = records.filter(
    ({ type }) => type === 'bench:diagnostic');
  const diagnostics = diagnosticRecords.map(({ data }) => data);

  assert.strictEqual(completedResult.error, undefined);
  assert.strictEqual(failedResult.error, expectedError);
  assert.strictEqual(diagnostics.length, 4);
  assert.strictEqual(namedDiagnostics.length, diagnostics.length);
  assert.deepStrictEqual(diagnostics.map(({ message }) => message), [
    'warmup 0',
    'measurement 0',
    'measurement 1',
    'before failure',
  ]);
  assert.deepStrictEqual(diagnostics.map(({ phase, index, level }) => ({
    phase,
    index,
    level,
  })), [
    { phase: 'warmup', index: 0, level: 'info' },
    { phase: 'measurement', index: 0, level: 'warning' },
    { phase: 'measurement', index: 1, level: 'warning' },
    { phase: 'measurement', index: 0, level: 'warning' },
  ]);
  assert.deepStrictEqual(
    diagnostics.map(({ detail }) => detail),
    [{ index: 0 }, { index: 0 }, { index: 1 }, { retained: true }]);
  assert(diagnostics.slice(0, 3).every(
    ({ benchId }) => benchId === completedResult.benchId));
  assert.strictEqual(diagnostics[3].benchId, failedResult.benchId);
  assert.notStrictEqual(namedDiagnostics[0], diagnostics[0]);

  for (let i = 0; i < 2; i++) {
    const diagnosticIndex = records.indexOf(diagnosticRecords[i + 1]);
    const sampleIndex = records.findIndex(({ type, data }) =>
      type === 'bench:sample' && data.benchId === completedResult.benchId &&
      data.index === i);
    assert(diagnosticIndex < sampleIndex);
  }
  assert.throws(() => finalContext.diagnostic('too late'), {
    code: 'ERR_INVALID_STATE',
  });
  await testListenerFailure(undefined);
  await testListenerFailure(new Error('callback failed'));
  await testAbortDuringDiagnosticDelivery();
  await testAfterEachFailurePrecedence();
})().then(common.mustCall());
