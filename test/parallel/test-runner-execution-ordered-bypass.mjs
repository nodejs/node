// Flags: --no-warnings

import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { test, run } from 'node:test';

const files = [
  fixtures.path('test-runner', 'execution-ordered-bypass', 'slow.mjs'),
  fixtures.path('test-runner', 'execution-ordered-bypass', 'fast-fail.mjs'),
];

test('execution-ordered events bypass FileTest declaration-order buffer', async () => {
  // Concurrency must be a number so the runner does not collapse it to 1 on
  // single-core CI runners (where `concurrency: true` resolves to
  // `availableParallelism() - 1`). Without two slots the runner spawns the
  // files sequentially and fast-fail never starts while slow is sleeping.
  const stream = run({
    files,
    isolation: 'process',
    concurrency: 2,
  });

  const events = [];

  stream.on('test:complete', (data) => {
    if (data.name === 'slow' || data.name === 'fast-fail') {
      events.push(`complete:${data.name}`);
    }
  });

  stream.on('test:fail', (data) => {
    if (data.name === 'fast-fail') {
      events.push(`fail:${data.name}`);
    }
  });

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);

  const completeFast = events.indexOf('complete:fast-fail');
  const completeSlow = events.indexOf('complete:slow');
  const failFast = events.indexOf('fail:fast-fail');

  assert.notStrictEqual(completeFast, -1);
  assert.notStrictEqual(completeSlow, -1);
  assert.notStrictEqual(failFast, -1);

  assert.ok(
    completeFast < completeSlow,
    `test:complete for fast-fail should arrive before slow; events=${events.join(', ')}`,
  );

  // test:fail is declaration-ordered, so the bypass must not affect it.
  assert.ok(
    failFast > completeSlow,
    `test:fail for fast-fail should arrive after test:complete for slow; events=${events.join(', ')}`,
  );
});
