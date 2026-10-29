'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const { test } = require('node:test');

test('mock timers do not break test timeout cleanup', async () => {
  const fixture = fixtures.path('test-runner', 'mock-timers-with-timeout.js');
  spawnSyncAndExitWithoutError(process.execPath, ['--test', fixture], {
    timeout: 30_000,
  });
});
