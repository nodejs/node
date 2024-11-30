'use strict';

require('../common');
const assert = require('assert');
const test = require('node:test');
const { spawnSync, spawn } = require('child_process');
const { join } = require('path');
const fixtures = require('../common/fixtures');
const testFixtures = fixtures.path('test-runner');

test('testing with isolation enabled', () => {
  const args = ['--test', '--experimental-test-isolation=process'];
  const child = spawnSync(process.execPath, args, { cwd: join(testFixtures, 'worker-id') });

  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();

  assert.match(stdout, /NODE_TEST_WORKER_ID 1/);
  assert.match(stdout, /NODE_TEST_WORKER_ID 2/);
  assert.match(stdout, /NODE_TEST_WORKER_ID 3/);

  assert.strictEqual(child.status, 0);
});

test('testing with isolation disabled', () => {
  const args = ['--test', '--experimental-test-isolation=none'];
  const child = spawnSync(process.execPath, args, { cwd: join(testFixtures, 'worker-id') });

  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  const regex = /NODE_TEST_WORKER_ID 1/g;
  const result = stdout.match(regex);

  assert.strictEqual(result.length, 3);
  assert.strictEqual(child.status, 0);
});


test('testing with isolation enabled in watch mode', async () => {
  const args = ['--watch', '--test', '--experimental-test-isolation=process'];

  const child = spawn(process.execPath, args, { cwd: join(testFixtures, 'worker-id') });

  let outputData = '';
  let errorData = '';

  child.stdout.on('data', (data) => {
    outputData += data.toString();
  });

  child.stderr.on('data', (data) => {
    errorData += data.toString();
  });

  setTimeout(() => {
    child.kill();

    assert.strictEqual(errorData, '');
    assert.match(outputData, /NODE_TEST_WORKER_ID 1/);
    assert.match(outputData, /NODE_TEST_WORKER_ID 2/);
    assert.match(outputData, /NODE_TEST_WORKER_ID 3/);

    if (child.exitCode !== null) {
      assert.strictEqual(child.exitCode, null); // Since we killed it manually
    }
  }, 1000);

});

test('testing with isolation disabled in watch mode', async () => {
  const args = ['--watch', '--test', '--experimental-test-isolation=none'];

  const child = spawn(process.execPath, args, { cwd: join(testFixtures, 'worker-id') });

  let outputData = '';
  let errorData = '';

  child.stdout.on('data', (data) => {
    outputData += data.toString();
  });

  child.stderr.on('data', (data) => {
    errorData += data.toString();
  });

  setTimeout(() => {
    child.kill();

    assert.strictEqual(errorData, '');
    const regex = /NODE_TEST_WORKER_ID 1/g;
    const result = outputData.match(regex);

    assert.strictEqual(result.length, 3);

    if (child.exitCode !== null) {
      assert.strictEqual(child.exitCode, null);
    }
  }, 1000);

});
