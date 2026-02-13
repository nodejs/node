'use strict';
require('../common');
const { test } = require('node:test');
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const fixtures = require('../common/fixtures');

const runTest = async (args, cwd = process.cwd()) => {
  const child = spawn(process.execPath, args, {
    cwd,
    stdio: 'pipe',
    env: { ...process.env },
  });

  const stdout = [];
  const stderr = [];

  child.stdout.on('data', (chunk) => stdout.push(chunk));
  child.stderr.on('data', (chunk) => stderr.push(chunk));

  const [code] = await once(child, 'exit');

  return {
    code,
    stdout: Buffer.concat(stdout).toString('utf8'),
    stderr: Buffer.concat(stderr).toString('utf8'),
  };
};

test('bail stops test execution on first failure with isolation=process', async () => {
  const fixturesDir = fixtures.path('test-runner', 'bail');
  const result = await runTest([
    '--test-bail',
    '--test-concurrency=1', // <-- fixed concurrency to ensure predictable order of test execution
    '--test',
    'bail-test-1-pass.js',
    'bail-test-2-fail.js',
    'bail-test-3-pass.js',
    'bail-test-4-pass.js',
  ], fixturesDir);

  // Should exit with non-zero code due to failure
  assert.notStrictEqual(result.code, 0);

  // bail-test-1-pass.js should run (2 tests)
  assert.match(result.stdout, /test 1 passes/);
  assert.match(result.stdout, /test 2 passes/);

  // bail-test-2-fail.js should run and fail
  assert.match(result.stdout, /failing test 1/);

  // Files after failure should NOT run
  assert.doesNotMatch(result.stdout, /test 3 passes/);
  assert.doesNotMatch(result.stdout, /test 4 passes/);
  assert.doesNotMatch(result.stdout, /test 5 passes/);
});

test.todo('bail stops test execution on first failure with isolation=none');

test('bail stops test execution on first failure with isolation=none', async () => {
  const fixturesDir = fixtures.path('test-runner', 'bail');
  const result = await runTest([
    '--test-bail',
    '--test-reporter=spec',
    '--test-concurrency=1', // <-- fixed concurrency to ensure predictable order of test execution
    '--test-isolation=none',
    '--test',
    'bail-test-1-pass.js',
    'bail-test-2-fail.js',
    'bail-test-3-pass.js',
    'bail-test-4-pass.js',
  ], fixturesDir);

  // Should exit with non-zero code due to failure
  assert.notStrictEqual(result.code, 0);

  // bail-test-1-pass.js should run (2 tests)
  assert.match(result.stdout, /✔ test 1 passes/);
  assert.match(result.stdout, /✔ test 2 passes/);

  // bail-test-2-fail.js first test should run and fail
  assert.match(result.stdout, /✖ failing test 1/);

  // Bail event should be emitted
  assert.match(result.stdout, /Bailing out!/);

  // Remaining tests after failure should be cancelled
  assert.match(result.stdout, /✖ failing test 2/);
  assert.match(result.stdout, /✖ test 3 passes/);
  assert.match(result.stdout, /✖ test 4 passes/);
  assert.match(result.stdout, /✖ test 5 passes/);

  // Verify counters: 2 passed, 1 failed, 4 cancelled
  assert.match(result.stdout, /pass 2/);
  assert.match(result.stdout, /fail 1/);
  assert.match(result.stdout, /cancelled 4/);
});

test('bail throws error when combined with watch mode', async () => {
  const fixturesDir = fixtures.path('test-runner', 'bail');
  const result = await runTest([
    '--test',
    '--test-bail',
    '--watch',
    'bail-test-1-pass.js',
  ], fixturesDir);

  // Should exit with error
  assert.notStrictEqual(result.code, 0);

  // Should contain error message about bail+watch incompatibility
  assert.match(result.stderr, /not supported with watch mode/);
});

test('without bail, all tests run even after failure', async () => {
  const fixturesDir = fixtures.path('test-runner', 'bail');
  const result = await runTest([
    '--test',
    'bail-test-1-pass.js',
    'bail-test-2-fail.js',
    'bail-test-3-pass.js',
  ], fixturesDir);

  // Should exit with non-zero code due to failure
  assert.notStrictEqual(result.code, 0);

  // All tests should run
  assert.match(result.stdout, /test 1 passes/);
  assert.match(result.stdout, /test 2 passes/);
  assert.match(result.stdout, /failing test 1/);
  assert.match(result.stdout, /test 3 passes/);
  assert.match(result.stdout, /test 4 passes/);

  // Should NOT contain bail event
  assert.doesNotMatch(result.stdout, /test:bail/);
});
