'use strict';

const common = require('../common');

const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const fixtures = require('../common/fixtures');

const fixture =
  fixtures.path('test-runner/force-exit-inherited-stdio.js');

const maxDuration = common.platformTimeout(2000);
const stallDuration = common.platformTimeout(4000);
const start = Date.now();

const result = spawnSync(
  process.execPath,
  [
    '--test',
    '--test-force-exit',
    fixture,
  ],
  {
    encoding: 'utf8',
    env: {
      ...process.env,
      TEST_RUNNER_STALL_MS: String(stallDuration),
    },
  },
);

const duration = Date.now() - start;

assert.strictEqual(result.status, 0);
assert.strictEqual(result.signal, null);
assert.strictEqual(result.stderr, '');

assert.ok(
  duration < maxDuration,
  `test runner took ${duration}ms to exit`,
);
