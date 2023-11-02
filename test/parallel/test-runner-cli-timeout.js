'use strict';
require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

test('test-timeout flag', () => {
  const args = ['--test', '--test-timeout', 10, 'test/fixtures/test-runner/never_ending_sync.js'];
  const child = spawnSync(process.execPath, args, { timeout: 500 });

  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert.match(stdout, /not ok 1 - test/);
  assert.match(stdout, / {2}---/);
  assert.match(stdout, / {2}duration_ms: .*/);
  assert.match(stdout, /failureType: 'testTimeoutFailure/);
  assert.match(stdout, /error: 'test timed out after 10ms'/);
});
