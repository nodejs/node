'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { spawnSync, spawn } = require('child_process');

if (process.argv[2] === 'child') {
  const test = require('node:test');

  if (process.argv[3] === 'pass') {
    test('passing test', () => {
      assert.strictEqual(true, true);
    });
  } else if (process.argv[3] === 'fail') {
    assert.strictEqual(process.argv[3], 'fail');
    test('failing test', () => {
      assert.strictEqual(true, false);
    });
  } else assert.fail('unreachable');
} else {
  let child = spawnSync(process.execPath, [__filename, 'child', 'pass']);
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);

  child = spawnSync(process.execPath, ['--test', fixtures.path('test-runner', 'subdir', 'subdir_test.js')]);
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);

  child = spawnSync(process.execPath, [__filename, 'child', 'fail']);
  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);

  let stdout = '';
  child = spawn(process.execPath, ['--test', fixtures.path('test-runner', 'never_ending.js')]);
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => {
    if (!stdout.length) child.kill('SIGINT');
    stdout += chunk;
  })
  child.once('exit', common.mustCall(async (code, signal) => {
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    if (common.isWindows) {
      common.printSkipMessage('signals are not supported in windows');
    } else {
      assert.match(stdout, /not ok 1/);
      assert.match(stdout, /# cancelled 1\n/);
    }
  }));
}
