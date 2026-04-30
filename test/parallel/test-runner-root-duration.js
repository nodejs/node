'use strict';
const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { test } = require('node:test');

test('root duration is longer than test duration', async () => {
  const {
    code,
    stderr,
    stdout,
  } = await spawnPromisified(process.execPath, [
    '--test-reporter=tap',
    fixtures.path('test-runner/root-duration.mjs'),
  ]);

  assert.strictEqual(code, 0);
  assert.strictEqual(stderr, '');
  const durations = [...stdout.matchAll(/duration_ms:? ([.\d]+)/g)];
  assert.strictEqual(durations.length, 2);
  const testDuration = Number.parseFloat(durations[0][1]);
  const rootDuration = Number.parseFloat(durations[1][1]);
  assert.strictEqual(Number.isNaN(testDuration), false);
  assert.strictEqual(Number.isNaN(rootDuration), false);
  assert.strictEqual(rootDuration >= testDuration, true);
});
