// Flags: --no-warnings

import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { test, run } from 'node:test';

const fixture = fixtures.path(
  'test-runner',
  'console-output-attribution.mjs',
);

function findOutput(events, type, message) {
  return events.findIndex((event) =>
    event.type === type && event.data.message === message);
}

function assertAttributedOutput(events, type, message, name) {
  const dequeueIndex = events.findIndex((event) =>
    event.type === 'test:dequeue' && event.data.name === name);
  const outputIndex = findOutput(events, type, message);

  assert.notStrictEqual(dequeueIndex, -1);
  assert.notStrictEqual(outputIndex, -1);
  assert.ok(dequeueIndex < outputIndex);

  // Attributed output replaces the raw write, so it must be reported exactly
  // once through the attributed path and must not reach the file scoped one.
  // Raw events can be merged by the pipe, so the raw side is checked as a
  // single joined string rather than per event.
  const attributed = events.filter((event) =>
    event.type === type && 'testId' in event.data &&
    event.data.message === message);
  assert.strictEqual(attributed.length, 1);

  const rawOutput = events
    .filter((event) => event.type === type && !('testId' in event.data))
    .map((event) => event.data.message)
    .join('');
  assert.ok(!rawOutput.includes(message));

  const dequeue = events[dequeueIndex].data;
  const output = events[outputIndex].data;

  // 'file' is documented as a string for these events. Attributed output must
  // not weaken that, even for a test with no known source location.
  assert.strictEqual(typeof output.file, 'string');

  for (const key of [
    'name',
    'nesting',
    'testId',
    'parentId',
    'file',
    'line',
    'column',
    'entryFile',
  ]) {
    assert.strictEqual(output[key], dequeue[key]);
  }
}

function assertUnattributedOutput(events, type, message) {
  const output = events
    .filter((event) =>
      event.type === type && !('testId' in event.data))
    .map((event) => event.data.message)
    .join('');

  assert.ok(output.includes(message));
}

test('global console output is attributed to the current test', async () => {
  const stream = run({
    files: [fixture],
    isolation: 'process',
  });
  const events = [];

  for await (const event of stream) {
    if (
      event.type === 'test:dequeue' ||
      event.type === 'test:stdout' ||
      event.type === 'test:stderr'
    ) {
      events.push(event);
    }
  }

  assertAttributedOutput(
    events,
    'test:stdout',
    'suitehook-console-4f0a\n',
    'concurrent tests',
  );
  assertAttributedOutput(
    events,
    'test:stdout',
    'beforeeach-console-first-6c2b\n',
    'concurrent tests',
  );
  assertAttributedOutput(
    events,
    'test:stdout',
    'beforeeach-console-second-6c2b\n',
    'concurrent tests',
  );
  assertAttributedOutput(
    events,
    'test:stdout',
    'attributed-out-first-a17e\n',
    'first',
  );
  assertAttributedOutput(
    events,
    'test:stderr',
    'attributed-err-first-b85f\n',
    'first',
  );
  assertAttributedOutput(
    events,
    'test:stdout',
    'attributed-out-second-c62d\n',
    'second',
  );

  // A test with no known source location still reports a file, so that the
  // documented type of 'file' holds for every attributed event. The event has
  // to be the attributed one: the raw path also reports the file it came from,
  // so checking the file alone would pass even if attribution never happened.
  const noLocIndex = findOutput(
    events,
    'test:stdout',
    'attributed-out-noloc-5ad9\n',
  );
  const noLocDequeueIndex = events.findIndex((event) =>
    event.type === 'test:dequeue' &&
    event.data.name === 'no source location');

  assert.notStrictEqual(noLocIndex, -1);
  assert.notStrictEqual(noLocDequeueIndex, -1);
  assert.ok(noLocDequeueIndex < noLocIndex);

  const noLoc = events[noLocIndex].data;
  const noLocDequeue = events[noLocDequeueIndex].data;

  assert.ok('testId' in noLoc);
  assert.strictEqual(noLoc.name, 'no source location');
  assert.strictEqual(noLoc.testId, noLocDequeue.testId);
  assert.strictEqual(noLoc.parentId, noLocDequeue.parentId);
  assert.strictEqual(noLoc.nesting, noLocDequeue.nesting);
  assert.strictEqual(noLoc.entryFile, noLocDequeue.entryFile);
  assert.strictEqual(typeof noLoc.file, 'string');
  assert.strictEqual(noLoc.file, noLoc.entryFile);
  assert.strictEqual(noLoc.line, undefined);
  assert.strictEqual(noLoc.column, undefined);

  const noLocRaw = events
    .filter((event) =>
      event.type === 'test:stdout' && !('testId' in event.data))
    .map((event) => event.data.message)
    .join('');

  assert.ok(!noLocRaw.includes('attributed-out-noloc-5ad9\n'));

  assertUnattributedOutput(
    events,
    'test:stdout',
    'toplevel-console-7b31\n',
  );
  assertUnattributedOutput(
    events,
    'test:stdout',
    'suitedef-console-91de\n',
  );
  assertUnattributedOutput(
    events,
    'test:stdout',
    'globalhook-console-52ac\n',
  );
  assertUnattributedOutput(
    events,
    'test:stdout',
    'late-console-f08b\n',
  );
  assertUnattributedOutput(
    events,
    'test:stdout',
    'directstream-out-first-d40c\n',
  );
  assertUnattributedOutput(
    events,
    'test:stderr',
    'directstream-err-first-e93a\n',
  );
  assertUnattributedOutput(
    events,
    'test:stdout',
    'customconsole-out-3e75\n',
  );
});

test('console output is not attributed without process isolation', async () => {
  // A stray NODE_TEST_CONTEXT value must not turn on attribution, otherwise the
  // documented "process isolation only" behavior would not hold.
  const { stdout } = await common.spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('test-runner', 'console-output-attribution-no-isolation.mjs'),
  ], { env: { ...process.env, NODE_TEST_CONTEXT: 'not-a-child' } });

  assert.match(stdout, /__attributed_count__:0\n/);
  // The writes still have to reach stdout. Suppressing them would trade a
  // wrong attribution for lost output.
  assert.match(stdout, /attributed-out-first-a17e/);
});

test('assigning NODE_TEST_CONTEXT mid-run does not enable attribution', async () => {
  // Attribution is decided once during setup. If it were re-read per write,
  // test code could switch it on in a process that has no reporter able to
  // carry the attributed event, and the write would be dropped.
  const { stdout } = await common.spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('test-runner', 'console-output-attribution-env-mutation.mjs'),
  ]);

  assert.match(stdout, /__attributed_count__:0\n/);
  assert.match(stdout, /env-mutated-out-2d9c/);
});

test('the hook does not mutate the global console', async () => {
  // The hook is kept in module private storage, so the console object must not
  // gain a new symbol and freezing it must not break the test runner.
  assert.ok(!Object.getOwnPropertySymbols(console).some((symbol) =>
    String(symbol).includes('WriteToConsoleHook')));

  const { code, stderr } = await common.spawnPromisified(process.execPath, [
    '--no-warnings',
    '--require',
    fixtures.path('test-runner', 'freeze-console.js'),
    '--test',
    fixtures.path('test-runner', 'console-output-attribution-frozen-console.mjs'),
  ]);

  assert.strictEqual(code, 0);
  assert.doesNotMatch(stderr, /not extensible/);
});
