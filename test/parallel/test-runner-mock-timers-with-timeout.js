'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

test('mock timers do not break test timeout cleanup', async () => {
  const fixture = fixtures.path('test-runner', 'mock-timers-with-timeout.js');
  const cp = spawnSync(process.execPath, ['--test', fixture], {
    timeout: 30_000,
  });
  assert.strictEqual(cp.status, 0, `Test failed:\nstdout: ${cp.stdout}\nstderr: ${cp.stderr}`);
});
