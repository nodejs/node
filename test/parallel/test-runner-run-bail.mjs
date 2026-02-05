import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { test, run } from 'node:test';
import * as fixtures from '../common/fixtures.mjs';

test('bail option validation', async () => {
  // Test that bail rejects watch mode
  assert.throws(() => {
    run({ bail: true, watch: true, files: [] });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /not supported with watch mode/,
  });
});

test('bail stops test execution on first failure', async () => {
  const files = [
    fixtures.path('test-runner', 'bail', 'bail-test-1-pass.js'),
    fixtures.path('test-runner', 'bail', 'bail-test-2-fail.js'),
    fixtures.path('test-runner', 'bail', 'bail-test-3-pass.js'),
    fixtures.path('test-runner', 'bail', 'bail-test-4-pass.js'),
  ];

  const stream = run({ bail: true, concurrency: 1, files });

  let testCount = 0;
  let failCount = 0;

  stream.on('test:bail', common.mustCall((data) => {
    assert.ok(data.file);
  }));

  stream.on('test:pass', () => {
    testCount++;
  });

  stream.on('test:fail', () => {
    failCount++;
  });

  // eslint-disable-next-line no-unused-vars, no-empty
  for await (const event of stream) {}

  assert.strictEqual(failCount, 2);
  assert.strictEqual(testCount, 2);
});

test('without bail, all tests run even after failure', async () => {
  const files = [
    fixtures.path('test-runner', 'bail', 'bail-test-1-pass.js'),
    fixtures.path('test-runner', 'bail', 'bail-test-2-fail.js'),
    fixtures.path('test-runner', 'bail', 'bail-test-3-pass.js'),
  ];

  const stream = run({ bail: false, files });

  let bailEventReceived = false;
  let testCount = 0;

  stream.on('test:bail', () => {
    bailEventReceived = true;
  });

  stream.on('test:pass', () => {
    testCount++;
  });

  // eslint-disable-next-line no-unused-vars, no-empty
  for await (const _ of stream) {}

  assert.strictEqual(bailEventReceived, false);
  assert.strictEqual(testCount, 4);
});
