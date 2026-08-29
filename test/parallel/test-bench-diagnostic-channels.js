// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const dc = require('diagnostics_channel');
const { createRunner } = require('node:bench');

const prefix = `node:bench:test:${process.pid}`;
const inheritedName = `${prefix}:inherited`;
const nestedName = `${prefix}:nested`;
const benchmarkName = `${prefix}:benchmark`;
const unlistedName = `${prefix}:unlisted`;
const symbolName = Symbol(`${prefix}:symbol`);
const inheritedChannel = dc.channel(inheritedName);
const nestedChannel = dc.channel(nestedName);
const benchmarkChannel = dc.channel(benchmarkName);
const unlistedChannel = dc.channel(unlistedName);
const symbolChannel = dc.channel(symbolName);

function recordSample(context) {
  context.record({ duration_ns: 1n, operations: 1 });
}

async function testCapture() {
  const runner = createRunner({ yieldBetweenSamples: false });
  runner.suite('outer', {
    diagnosticChannels: [inheritedName, symbolName, inheritedName],
  }, common.mustCall(() => {
    runner.suite('inner', {
      diagnosticChannels: [nestedName],
    }, common.mustCall(() => {
      runner.bench('captured', {
        diagnosticChannels: [benchmarkName, nestedName],
        samples: 1,
      }, common.mustCall((context) => {
        const message = { value: 1 };
        inheritedChannel.publish(message);
        message.value = 0;
        nestedChannel.publish({ value: 2 });
        benchmarkChannel.publish({ value: 3 });
        unlistedChannel.publish({ value: 4 });
        symbolChannel.publish({ value: 5 });
        recordSample(context);
      }));
    }));
  }));
  runner.bench('not captured', { samples: 1 }, common.mustCall((context) => {
    inheritedChannel.publish({ value: 6 });
    recordSample(context);
  }));

  const records = await runner.run().toArray();
  const plan = records.find(
    ({ type, data }) => type === 'bench:plan' && data.name === 'captured').data;
  assert.deepStrictEqual(plan.diagnosticChannels, [
    inheritedName,
    nestedName,
    benchmarkName,
  ]);
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic').map(({ data }) => data);
  assert.deepStrictEqual(diagnostics.map(({ message }) => message), [
    { name: inheritedName, message: { value: 1 } },
    { name: nestedName, message: { value: 2 } },
    { name: benchmarkName, message: { value: 3 } },
  ]);
  assert(diagnostics.every(({ level }) => level === 'info'));
  assert.strictEqual(inheritedChannel.hasSubscribers, false);
  assert.strictEqual(nestedChannel.hasSubscribers, false);
  assert.strictEqual(benchmarkChannel.hasSubscribers, false);
  assert.strictEqual(symbolChannel.hasSubscribers, false);
}

async function testUncloneableMessage() {
  const name = `${prefix}:uncloneable`;
  const channel = dc.channel(name);
  const runner = createRunner({ yieldBetweenSamples: false });
  runner.bench('uncloneable', {
    diagnosticChannels: [name],
    samples: 1,
  }, common.mustCall((context) => {
    channel.publish(() => {});
    recordSample(context);
  }));
  const records = await runner.run().toArray();
  const result = records.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(result.error.name, 'DataCloneError');
  assert.strictEqual(channel.hasSubscribers, false);
}

async function testAbortCleanup() {
  const name = `${prefix}:abort`;
  const channel = dc.channel(name);
  const controller = new AbortController();
  const runner = createRunner({ yieldBetweenSamples: false });
  runner.bench('abort', {
    diagnosticChannels: [name],
    samples: 1,
    signal: controller.signal,
  }, common.mustCall((context) => {
    controller.abort(new Error('stop'));
    assert.strictEqual(channel.hasSubscribers, false);
    channel.publish({ ignored: true });
    recordSample(context);
  }));
  const records = await runner.run().toArray();
  assert.strictEqual(
    records.some(({ type }) => type === 'bench:diagnostic'),
    false,
  );
}

(async () => {
  await testCapture();
  await testUncloneableMessage();
  await testAbortCleanup();
})().then(common.mustCall());
