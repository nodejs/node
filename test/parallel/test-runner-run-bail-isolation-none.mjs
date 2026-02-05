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
  const stream = run({ bail: true, isolation: 'process', concurrency: 1, files });

  let testCount = 0;
  let failCount = 0;
  let bailCount = 0;

  stream.on('test:bail', common.mustCall(() => {
    bailCount++;
  }));

  stream.on('test:pass', () => {
    testCount++;
  });

  stream.on('test:fail', () => {
    failCount++;
  });

  // eslint-disable-next-line no-unused-vars, no-empty
  for await (const _ of stream) {}

  return { testCount, failCount, bailCount };
}

test('it should bail in isolation process mode', async () => {
  const { testCount, failCount, bailCount } = await runScenario();

  assert.strictEqual(failCount, 2);
  assert.strictEqual(testCount, 2);
  assert.strictEqual(bailCount, 1);
});
