'use strict';
require('../common');
const assert = require('node:assert');
const { run } = require('node:test');
const fixtures = require('../common/fixtures');

async function collectEvents() {
  const events = [];
  const stream = run({
    files: [fixtures.path('test-runner/test-id-fixture.js')],
    isolation: 'none',
  });
  for await (const event of stream) {
    events.push(event);
  }
  return events;
}

async function main() {
  const events = await collectEvents();

  // 1. Every per-test event should have a numeric testId.
  const perTestTypes = new Set([
    'test:start', 'test:complete', 'test:fail',
    'test:pass', 'test:enqueue', 'test:dequeue',
  ]);
  for (const event of events) {
    if (perTestTypes.has(event.type)) {
      assert.strictEqual(typeof event.data.testId, 'number',
                         `${event.type} for "${event.data.name}" should have numeric testId`);
    }
  }

  // 2. test:start and test:fail for the same instance should share testId.
  const failEvent = events.find(
    (e) => e.type === 'test:fail' && e.data.name === 'e2e',
  );
  assert.ok(failEvent, 'should have a test:fail for "e2e"');

  const startEvent = events.find(
    (e) => e.type === 'test:start' &&
           e.data.testId === failEvent.data.testId,
  );
  assert.ok(startEvent, 'should have a test:start with matching testId');
  assert.strictEqual(startEvent.data.name, 'e2e');

  // 3. Concurrent instances at the same source location get distinct testIds.
  const e2eStarts = events.filter(
    (e) => e.type === 'test:start' && e.data.name === 'e2e',
  );
  assert.strictEqual(e2eStarts.length, 4);

  const testIds = e2eStarts.map((e) => e.data.testId);
  const uniqueIds = new Set(testIds);
  assert.strictEqual(uniqueIds.size, 4,
                     `all 4 "e2e" instances should have distinct testIds, got: ${testIds}`);

  // 4. test:complete for the same instance shares testId with test:start.
  const completeEvents = events.filter(
    (e) => e.type === 'test:complete' && e.data.name === 'e2e',
  );
  for (const complete of completeEvents) {
    const matchingStart = e2eStarts.find(
      (s) => s.data.testId === complete.data.testId,
    );
    assert.ok(matchingStart,
              `test:complete (testId=${complete.data.testId}) should match a test:start`);
  }

  console.log('All testId assertions passed');
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
