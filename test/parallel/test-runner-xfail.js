'use strict';
const common = require('../common');
const { test } = require('node:test');
const { spawn } = require('child_process');
const assert = require('node:assert');

if (process.env.CHILD_PROCESS === 'true') {
  test('fail with message string', { expectFailure: 'reason string' }, () => {
    assert.fail('boom');
  });

  test('fail with message object', { expectFailure: { message: 'reason object' } }, () => {
    assert.fail('boom');
  });

  test('fail with validation regex', { expectFailure: { with: /boom/ } }, () => {
    assert.fail('boom');
  });

  test('fail with validation object', { expectFailure: { with: { message: 'boom' } } }, () => {
    assert.fail('boom');
  });

  test('fail with validation error (wrong error)', { expectFailure: { with: /bang/ } }, () => {
    assert.fail('boom'); // Should result in real failure because error doesn't match
  });

  test('unexpected pass', { expectFailure: true }, () => {
    // Should result in real failure because it didn't fail
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
    // We expect exit code 1 because 'unexpected pass' and 'wrong error' should fail the test run
    assert.strictEqual(code, 1);

    // Check outputs
    assert.match(stdout, /# EXPECTED FAILURE reason string/);
    assert.match(stdout, /# EXPECTED FAILURE reason object/);
    assert.match(stdout, /not ok \d+ - fail with validation error \(wrong error\)/);
    assert.match(stdout, /not ok \d+ - unexpected pass/);
  }));
}
