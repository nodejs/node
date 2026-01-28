'use strict';
const common = require('../common');
const { test } = require('node:test');
const { spawn } = require('child_process');
const assert = require('node:assert');

if (process.env.CHILD_PROCESS === 'true') {
  test('fail with message', { expectFailure: 'flaky test reason' }, () => {
    assert.fail('boom');
  });
} else {
  const child = spawn(process.execPath, ['--test-reporter', 'tap', __filename], {
    env: { ...process.env, CHILD_PROCESS: 'true' },
    stdio: 'pipe',
  });

  let stdout = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => { stdout += chunk; });

  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    assert.match(stdout, /# EXPECTED FAILURE flaky test reason/);
  }));
}
