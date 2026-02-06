import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { test, run } from 'node:test';
import * as fixtures from '../common/fixtures.mjs';

const files = [
  fixtures.path('test-runner', 'bail', 'bail-test-1-pass.js'),
  fixtures.path('test-runner', 'bail', 'bail-test-2-fail.js'),
  fixtures.path('test-runner', 'bail', 'bail-test-3-pass.js'),
  fixtures.path('test-runner', 'bail', 'bail-test-4-pass.js'),
];

async function runScenario() {
  const stream = run({ bail: true, isolation: 'none', concurrency: 1, files });

  let testCount = 0;
  let failCount = 0;
  let bailCount = 0;
  let bailedOutCount = 0;

  stream.on('test:bail', common.mustCall(() => {
    bailCount++;
  }));

  stream.on('test:pass', () => {
    testCount++;
  });

  stream.on('test:fail', (item) => {
    if (item.details?.error?.failureType === 'bailedOut') {
      bailedOutCount++;
    } else {
      failCount++;
    }
  });

  // eslint-disable-next-line no-unused-vars, no-empty
  for await (const _ of stream) {}

  return { testCount, failCount, bailCount, bailedOutCount };
}

test('it should bail in isolation none mode', async () => {
  const { testCount, failCount, bailCount, bailedOutCount } = await runScenario();

  // 2 tests pass (from bail-test-1-pass.js)
  assert.strictEqual(testCount, 2);
  // 1 test actually fails (first test in bail-test-2-fail.js)
  assert.strictEqual(failCount, 1);
  // 1 bail event is emitted
  assert.strictEqual(bailCount, 1);
  // 4 tests are cancelled due to bailout
  // (failing test 2, test 3 passes, test 4 passes, test 5 passes)
  assert.strictEqual(bailedOutCount, 4);
});
