'use strict';
require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const dc = require('node:diagnostics_channel');
const { describe, it, test } = require('node:test');
const { spawnSync } = require('child_process');
const { join } = require('path');

const events = [];

dc.subscribe('tracing:node.test:start', (data) => events.push({ event: 'start', name: data.name }));
dc.subscribe('tracing:node.test:end', (data) => events.push({ event: 'end', name: data.name }));
dc.subscribe('tracing:node.test:error', (data) => events.push({ event: 'error', name: data.name }));

test('passing test fires start and end', async () => {});

// Validate events were emitted (check after all tests via process.on('exit'))
process.on('exit', () => {
  // Check passing test
  const testName1 = 'passing test fires start and end';
  const startEvents = events.filter((e) => e.event === 'start' && e.name === testName1);
  const endEvents = events.filter((e) => e.event === 'end' && e.name === testName1);
  assert.strictEqual(startEvents.length, 1);
  assert.strictEqual(endEvents.length, 1);

  // Check nested tests fire events
  const nested1Start = events.filter((e) => e.event === 'start' && e.name === 'nested test 1');
  const nested1End = events.filter((e) => e.event === 'end' && e.name === 'nested test 1');
  const nested2Start = events.filter((e) => e.event === 'start' && e.name === 'nested test 2');
  const nested2End = events.filter((e) => e.event === 'end' && e.name === 'nested test 2');
  assert.strictEqual(nested1Start.length, 1);
  assert.strictEqual(nested1End.length, 1);
  assert.strictEqual(nested2Start.length, 1);
  assert.strictEqual(nested2End.length, 1);

  // Check describe block tests fire events
  const describeStart = events.filter((e) => e.event === 'start' && e.name === 'test inside describe');
  const describeEnd = events.filter((e) => e.event === 'end' && e.name === 'test inside describe');
  const describeStart2 = events.filter(
    (e) => e.event === 'start' && e.name === 'another test inside describe',
  );
  const describeEnd2 = events.filter(
    (e) => e.event === 'end' && e.name === 'another test inside describe',
  );
  assert.strictEqual(describeStart.length, 1);
  assert.strictEqual(describeEnd.length, 1);
  assert.strictEqual(describeStart2.length, 1);
  assert.strictEqual(describeEnd2.length, 1);

  // Check async operations test fires events
  const asyncTestName = 'context is available in async operations within test';
  const asyncStart = events.filter((e) => e.event === 'start' && e.name === asyncTestName);
  const asyncEnd = events.filter((e) => e.event === 'end' && e.name === asyncTestName);
  assert.strictEqual(asyncStart.length, 1);
  assert.strictEqual(asyncEnd.length, 1);
});

// Test bindStore context propagation
const testStorage = new AsyncLocalStorage();

// bindStore on the start channel: whenever a test fn runs, set testStorage to the test name
dc.channel('tracing:node.test:start').bindStore(testStorage, (data) => data.name);

const expectedName = 'bindStore propagates into test body via start channel';
test(expectedName, async () => {
  const storedValueDuringTest = testStorage.getStore();
  assert.strictEqual(storedValueDuringTest, expectedName);

  // Propagates into async operations inside the test
  const valueInSetImmediate = await new Promise((resolve) => {
    setImmediate(() => resolve(testStorage.getStore()));
  });
  assert.strictEqual(valueInSetImmediate, expectedName);
});

test('bindStore value is isolated between tests', async () => {
  assert.strictEqual(testStorage.getStore(), 'bindStore value is isolated between tests');
});

test('nested tests fire events with correct names', async (t) => {
  await t.test('nested test 1', async () => {
    const stored = testStorage.getStore();
    assert.strictEqual(stored, 'nested test 1');
  });

  await t.test('nested test 2', async () => {
    const stored = testStorage.getStore();
    assert.strictEqual(stored, 'nested test 2');
  });
});

describe('describe block with tests', () => {
  it('test inside describe', async () => {
    const stored = testStorage.getStore();
    assert.strictEqual(stored, 'test inside describe');
  });

  it('another test inside describe', async () => {
    const stored = testStorage.getStore();
    assert.strictEqual(stored, 'another test inside describe');
  });
});

test('context is available in async operations within test', async () => {
  const testName = 'context is available in async operations within test';
  assert.strictEqual(testStorage.getStore(), testName);

  // Verify context is available in setImmediate
  const valueInImmediate = await new Promise((resolve) => {
    setImmediate(() => resolve(testStorage.getStore()));
  });
  assert.strictEqual(valueInImmediate, testName);

  // Verify context is available in setTimeout
  const valueInTimeout = await new Promise((resolve) => {
    setTimeout(() => resolve(testStorage.getStore()), 0);
  });
  assert.strictEqual(valueInTimeout, testName);
});

test('error events fire for failing tests in fixture', async () => {
  // Run the fixture test that intentionally fails
  const fixturePath = join(__dirname, '../fixtures/test-runner/diagnostics-channel-error-test.js');
  const result = spawnSync(process.execPath, [fixturePath], { encoding: 'utf8' });

  // The fixture test intentionally fails, so exit code should be non-zero
  assert.notStrictEqual(result.status, 0);

  // Extract and verify error events from fixture output
  // The fixture outputs JSON with errorEvents array on exit
  const lines = result.stdout.split('\n');
  const eventLine = lines.find((line) => line.includes('errorEvents'));
  assert.ok(eventLine, 'Expected errorEvents line in fixture output');
  const { errorEvents } = JSON.parse(eventLine);
  assert.strictEqual(errorEvents.includes('test that intentionally fails'), true);
});
