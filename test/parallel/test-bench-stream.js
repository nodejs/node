// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { createRunner } = require('node:bench');
const { setImmediate, setTimeout } = require('timers/promises');

function recordSample(b) {
  b.record({
    __proto__: null,
    operations: 1,
    duration_ns: 1n,
  });
}

async function testReadableBackpressure() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const sampleCount = 64;
  let calls = 0;
  const completion = runner.bench('bounded stream', {
    samples: sampleCount,
  }, (b) => {
    calls++;
    recordSample(b);
  });
  const stream = runner.run();
  const iterator = stream[Symbol.asyncIterator]();
  const first = await iterator.next();

  assert.strictEqual(first.value.type, 'bench:plan');
  await setImmediate();
  assert(calls < sampleCount);
  assert(stream.readableLength <= stream.readableHighWaterMark);

  const records = [first.value];
  for (;;) {
    const next = await iterator.next();
    if (next.done) break;
    records.push(next.value);
  }

  const result = await completion;
  assert.strictEqual(calls, sampleCount);
  assert.strictEqual(result.samples.length, sampleCount);
  assert.strictEqual(records[1].type, 'bench:start');
  assert.strictEqual(records.length, sampleCount + 4);
}

async function testPlanBackpressure() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const benchmarkCount = 32;
  const completions = [];
  let calls = 0;
  for (let i = 0; i < benchmarkCount; i++) {
    completions.push(runner.bench(`planned ${i}`, { samples: 1 }, (b) => {
      calls++;
      recordSample(b);
    }));
  }
  const stream = runner.run();
  const iterator = stream[Symbol.asyncIterator]();
  const first = await iterator.next();

  assert.strictEqual(first.value.type, 'bench:plan');
  await setImmediate();
  assert.strictEqual(calls, 0);
  assert(stream.readableLength <= stream.readableHighWaterMark);
  const records = [first.value];
  for (;;) {
    const next = await iterator.next();
    if (next.done) break;
    records.push(next.value);
  }

  await Promise.all(completions);
  assert.strictEqual(calls, benchmarkCount);
  assert.strictEqual(
    records.slice(0, benchmarkCount).every(({ type }) => type === 'bench:plan'),
    true,
  );
}

async function testDestroyWhileBlocked() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const completions = [];
  for (let i = 0; i < 32; i++) {
    completions.push(runner.bench(`destroyed ${i}`, {
      samples: 1,
    }, recordSample));
  }
  const stream = runner.run();
  const unblocked = stream.waitForDrain();
  const iterator = stream[Symbol.asyncIterator]();
  await iterator.next();
  await unblocked;
  for (let i = 0; i < 100 &&
       stream.readableLength < stream.readableHighWaterMark; i++) {
    await setImmediate();
  }
  assert.strictEqual(stream.readableLength, stream.readableHighWaterMark);

  const draining = stream.waitForDrain();
  const closed = new Promise((resolve) => stream.once('close', resolve));
  stream.destroy();
  await assert.rejects(draining, { code: 'ERR_INVALID_STATE' });
  await assert.rejects(stream.waitForDrain(), { code: 'ERR_INVALID_STATE' });
  await closed;
  await Promise.all(completions);
}

async function testNamedEventsWithoutReading() {
  const runner = createRunner();
  const sampleCount = 64;
  let calls = 0;
  const completion = runner.bench('named events', {
    samples: sampleCount,
  }, (b) => {
    calls++;
    recordSample(b);
  });
  const stream = runner.run();
  const summary = await new Promise((resolve) => {
    stream.once('bench:summary', resolve);
  });
  const result = await completion;

  assert.strictEqual(calls, sampleCount);
  assert.strictEqual(result.samples.length, sampleCount);
  assert.strictEqual(summary.success, true);
  assert.strictEqual(stream.readableLength, sampleCount + 4);
  assert(stream.readableLength > stream.readableHighWaterMark);
  stream.destroy();
}

async function testCancellationCompletesBenchmarks() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const first = runner.bench('cancelled stream', { samples: 64 }, recordSample);
  const second = runner.bench('continues headlessly', {
    samples: 1,
  }, recordSample);
  const stream = runner.run();
  const iterator = stream[Symbol.asyncIterator]();

  await iterator.next();
  await iterator.return();
  const results = await Promise.all([first, second]);
  assert.strictEqual(results[0].samples.length, 64);
  assert.strictEqual(results[1].samples.length, 1);
}

async function testDeliveryDoesNotConsumeTimeout() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const completion = runner.bench('slow consumer', {
    samples: 32,
    timeout: common.platformTimeout(20),
  }, recordSample);
  const stream = runner.run();
  const iterator = stream[Symbol.asyncIterator]();

  await iterator.next();
  await setTimeout(common.platformTimeout(50));
  for (;;) {
    const next = await iterator.next();
    if (next.done) break;
  }

  const result = await completion;
  assert.strictEqual(result.error, undefined);
  assert.strictEqual(result.samples.length, 32);
}

async function testReportingFailureSettlesBenchmarks() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const failure = new Error('record listener failed');
  const first = runner.bench('reported', { samples: 1 }, recordSample);
  const second = runner.bench('settled', { samples: 1 }, recordSample);
  const stream = runner.run();
  stream.once('bench:complete', common.mustCall(() => {
    throw failure;
  }));
  stream.resume();

  const results = await Promise.all([first, second]);
  assert.strictEqual(results[0].error, undefined);
  assert.strictEqual(results[1].error, failure);
  await setImmediate();
}

async function testSummaryListenerFailure() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const failure = new Error('summary listener failed');
  const completion = runner.bench('summary failure', {
    samples: 1,
  }, recordSample);
  const stream = runner.run();
  const diagnostics = [];
  stream.on('bench:diagnostic', (diagnostic) => {
    diagnostics.push(diagnostic);
  });
  stream.once('bench:summary', common.mustCall(() => {
    throw failure;
  }));
  const ended = new Promise((resolve) => stream.once('end', resolve));
  stream.resume();

  const result = await completion;
  await ended;
  assert.strictEqual(result.error, undefined);
  assert.strictEqual(diagnostics.length, 1);
  assert.strictEqual(diagnostics[0].error.message, failure.message);
}

async function testRecordOwnership() {
  const runner = createRunner({ yieldBetweenSamples: false });
  const expectedError = new Error('expected failure');
  expectedError.code = 'ERR_EXPECTED';
  expectedError.cause = expectedError;
  expectedError.uncloneable = new WeakMap();
  expectedError.context = {
    note: 'preserved',
    callback() {},
    get accessor() { return 'value'; },
  };
  expectedError.customContext = {
    __proto__: {},
    note: 'preserved',
  };
  expectedError.arrayPayload = [new WeakMap()];
  const innerError = new Error('inner failure');
  innerError.code = 'ERR_INNER';
  const aggregate = new AggregateError([innerError], 'aggregate failure');
  aggregate.code = 'ERR_AGGREGATE';
  const causedError = new Error('caused failure', { cause: aggregate });
  causedError.references = new Map([['self', causedError]]);
  causedError.members = new Set([causedError]);
  const measured = runner.bench('owned result', {
    params: { kind: 'original' },
    samples: 1,
  }, (b) => {
    b.record({
      __proto__: null,
      operations: 1,
      duration_ns: 1n,
      detail: { value: 'original' },
    });
  });
  const failed = runner.bench('owned error', { samples: 1 }, () => {
    throw expectedError;
  });
  const caused = runner.bench('owned cause', { samples: 1 }, () => {
    throw causedError;
  });
  const throwingNameError = new Error('throwing name');
  Object.defineProperty(throwingNameError, 'name', {
    configurable: true,
    get() { throw new Error('name getter'); },
  });
  const throwingName = runner.bench('throwing name', {
    samples: 1,
  }, () => {
    throw throwingNameError;
  });
  const thrownValue = new WeakMap();
  const uncloneable = runner.bench('uncloneable error', {
    samples: 1,
  }, () => {
    throw thrownValue;
  });
  const proxyError = new Proxy({}, {
    getPrototypeOf() {
      throw new Error('prototype trap');
    },
  });
  const trapped = runner.bench('trapping error', { samples: 1 }, () => {
    throw proxyError;
  });
  const afterTrap = runner.bench('after trapping error', {
    samples: 1,
  }, recordSample);
  const stream = runner.run();
  let eventSample;
  let eventComplete;
  let eventError;
  let eventSummary;

  stream.on('bench:sample', (sample) => {
    if (sample.name !== 'owned result') return;
    eventSample = sample;
    sample.name = 'changed by event';
    sample.detail.value = 'changed by event';
  });
  stream.on('bench:complete', (result) => {
    if (result.name === 'owned result') {
      eventComplete = result;
      result.params.kind = 'changed by event';
      result.samples[0].detail.value = 'changed by event';
    } else if (result.name === 'owned error') {
      eventError = result.error;
      result.error.code = 'ERR_CHANGED';
    }
  });
  stream.on('bench:summary', (summary) => {
    eventSummary = summary;
    summary.counts.total = 100;
  });

  const records = await stream.toArray();
  const measuredResult = await measured;
  const failedResult = await failed;
  await caused;
  const throwingNameResult = await throwingName;
  const uncloneableResult = await uncloneable;
  const trappedResult = await trapped;
  const afterTrapResult = await afterTrap;
  const streamSample = records.find(
    ({ type }) => type === 'bench:sample').data;
  const streamResults = records.filter(
    ({ type }) => type === 'bench:complete').map(({ data }) => data);
  const streamMeasured = streamResults.find(
    ({ name }) => name === 'owned result');
  const streamFailed = streamResults.find(
    ({ name }) => name === 'owned error');
  const streamCaused = streamResults.find(
    ({ name }) => name === 'owned cause');
  const streamThrowingName = streamResults.find(
    ({ name }) => name === 'throwing name');
  const streamSummary = records.find(
    ({ type }) => type === 'bench:summary').data;

  assert.notStrictEqual(eventSample, streamSample);
  assert.notStrictEqual(eventComplete, streamMeasured);
  assert.notStrictEqual(streamMeasured, measuredResult);
  assert.strictEqual(streamSample.name, 'owned result');
  assert.strictEqual(streamSample.detail.value, 'original');
  assert.strictEqual(streamMeasured.params.kind, 'original');
  assert.strictEqual(streamMeasured.samples[0].detail.value, 'original');
  assert.strictEqual(measuredResult.params.kind, 'original');
  assert.strictEqual(measuredResult.samples[0].detail.value, 'original');

  streamMeasured.samples[0].detail.value = 'changed by stream';
  assert.strictEqual(measuredResult.samples[0].detail.value, 'original');
  assert.notStrictEqual(eventError, streamFailed.error);
  assert.notStrictEqual(streamFailed.error, expectedError);
  assert.strictEqual(streamFailed.error.code, 'ERR_EXPECTED');
  assert.strictEqual(streamFailed.error.cause, streamFailed.error);
  assert.strictEqual(streamFailed.error.context.note, 'preserved');
  assert.strictEqual(streamFailed.error.context.callback, undefined);
  assert.strictEqual(streamFailed.error.context.accessor, undefined);
  assert.strictEqual(streamFailed.error.customContext.note, 'preserved');
  assert.deepStrictEqual(streamFailed.error.arrayPayload, [undefined]);
  assert.strictEqual(failedResult.error, expectedError);
  assert.strictEqual(failedResult.error.code, 'ERR_EXPECTED');
  assert.strictEqual(failedResult.error.cause, failedResult.error);
  assert.strictEqual(uncloneableResult.error, thrownValue);
  assert.strictEqual(trappedResult.error, proxyError);
  assert.strictEqual(afterTrapResult.error, undefined);
  assert.strictEqual(throwingNameResult.error, throwingNameError);
  assert.strictEqual(streamThrowingName.error.name, 'Error');
  assert.strictEqual(streamThrowingName.error.message, 'throwing name');
  assert(streamCaused.error.cause instanceof AggregateError);
  assert.strictEqual(streamCaused.error.cause.name, 'AggregateError');
  assert.strictEqual(streamCaused.error.cause.code, 'ERR_AGGREGATE');
  assert.strictEqual(streamCaused.error.cause.errors[0].code, 'ERR_INNER');
  assert.strictEqual(
    streamCaused.error.references.get('self'), streamCaused.error);
  assert.strictEqual(streamCaused.error.members.has(streamCaused.error), true);
  assert.notStrictEqual(eventSummary, streamSummary);
  assert.strictEqual(streamSummary.counts.total, 7);
}

(async () => {
  await testReadableBackpressure();
  await testPlanBackpressure();
  await testDestroyWhileBlocked();
  await testNamedEventsWithoutReading();
  await testCancellationCompletesBenchmarks();
  await testDeliveryDoesNotConsumeTimeout();
  await testReportingFailureSettlesBenchmarks();
  await testSummaryListenerFailure();
  await testRecordOwnership();
})().then(common.mustCall());
