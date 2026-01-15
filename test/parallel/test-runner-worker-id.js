'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

test('NODE_TEST_WORKER_ID is set for concurrent test files', async () => {
  const args = [
    '--test',
    fixtures.path('test-runner', 'worker-id', 'test-1.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-2.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-3.mjs'),
  ];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('NODE_TEST_WORKER_ID is set with explicit concurrency', async () => {
  const args = [
    '--test',
    '--test-concurrency=2',
    fixtures.path('test-runner', 'worker-id', 'test-1.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-2.mjs'),
  ];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('NODE_TEST_WORKER_ID is 1 with concurrency=1', async () => {
  const args = ['--test', '--test-concurrency=1', fixtures.path('test-runner', 'worker-id', 'test-1.mjs')];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('NODE_TEST_WORKER_ID with explicit isolation=process', async () => {
  const args = [
    '--test',
    '--test-isolation=process',
    fixtures.path('test-runner', 'worker-id', 'test-1.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-2.mjs'),
  ];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('NODE_TEST_WORKER_ID is 1 with isolation=none', async () => {
  const args = [
    '--test',
    '--test-isolation=none',
    fixtures.path('test-runner', 'worker-id', 'test-1.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-2.mjs'),
  ];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('context.workerId matches NODE_TEST_WORKER_ID', async () => {
  const args = ['--test', fixtures.path('test-runner', 'worker-id', 'test-1.mjs')];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  // The fixture tests already verify that context.workerId matches the env var
  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('worker IDs are reused when more tests than concurrency', async () => {
  const tmpdir = require('../common/tmpdir');
  const { writeFileSync } = require('node:fs');
  tmpdir.refresh();

  // Create 9 separate test files dynamically
  const testFiles = [];
  const usageFile = tmpdir.resolve('worker-usage.txt');
  for (let i = 1; i <= 9; i++) {
    const testFile = tmpdir.resolve(`reuse-test-${i}.mjs`);
    writeFileSync(
      testFile,
      `import { test } from 'node:test';
import { appendFileSync } from 'node:fs';

test('track worker ${i}', () => {
  const workerId = process.env.NODE_TEST_WORKER_ID;
  const usageFile = process.env.WORKER_USAGE_FILE;
  appendFileSync(usageFile, workerId + '\\n');
});
`,
    );
    testFiles.push(testFile);
  }

  const args = ['--test', '--test-concurrency=3', ...testFiles];
  const result = spawnSync(process.execPath, args, {
    env: { ...process.env, WORKER_USAGE_FILE: usageFile }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);

  // Read and analyze worker IDs used
  const { readFileSync } = require('node:fs');
  const workerIds = readFileSync(usageFile, 'utf8').trim().split('\n');

  // Count occurrences of each worker ID
  const workerCounts = {};
  workerIds.forEach((id) => {
    workerCounts[id] = (workerCounts[id] || 0) + 1;
  });

  const uniqueWorkers = Object.keys(workerCounts);
  assert.strictEqual(
    uniqueWorkers.length,
    3,
    `Should have exactly 3 unique worker IDs, got ${uniqueWorkers.length}: ${uniqueWorkers.join(', ')}`
  );

  Object.entries(workerCounts).forEach(([id, count]) => {
    assert.strictEqual(count, 3, `Worker ID ${id} should be used 3 times, got ${count}`);
  });
});
