import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it, beforeEach, afterEach, run } from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import path from 'node:path';

const testFixtures = fixtures.path('test-runner', 'global-setup-teardown');

describe('require(\'node:test\').run with global hooks', { concurrency: false }, () => {
  beforeEach(() => {
    tmpdir.refresh();
  });

  let originalEnv;

  beforeEach(() => {
    originalEnv = { ...process.env };
  });

  afterEach(() => {
    process.env = originalEnv;
  });

  async function runTestWithGlobalHooks({ globalSetupFile, testFile = 'test-file.js', env = {} }) {
    const testFilePath = path.join(testFixtures, testFile);
    const globalSetupPath = path.join(testFixtures, globalSetupFile);

    Object.entries(env).forEach(([key, value]) => {
      process.env[key] = value;
    });
    process.env.AVOID_PRINT_LOGS = true;

    const stream = run({
      files: [testFilePath],
      globalSetupPath
    });

    const results = { passed: 0, failed: 0 };
    stream.on('test:pass', () => { results.passed++; });
    stream.on('test:fail', () => { results.failed++; });

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);

    return { results };
  }

  it('should run globalSetup and globalTeardown functions', async () => {
    const setupFlagPath = tmpdir.resolve('setup-executed.tmp');
    const teardownFlagPath = tmpdir.resolve('teardown-executed.tmp');

    const { results } = await runTestWithGlobalHooks({
      globalSetupFile: 'basic-setup-teardown.js',
      env: {
        SETUP_FLAG_PATH: setupFlagPath,
        TEARDOWN_FLAG_PATH: teardownFlagPath
      }
    });

    assert.strictEqual(results.passed, 2);
    assert.strictEqual(results.failed, 0);
    // After all tests complete, the teardown should have run
    assert.ok(fs.existsSync(teardownFlagPath), 'Teardown flag file should exist');
    const content = fs.readFileSync(teardownFlagPath, 'utf8');
    assert.strictEqual(content, 'Teardown was executed');
    // Setup flag should have been removed by teardown
    assert.ok(!fs.existsSync(setupFlagPath), 'Setup flag file should have been removed');
  });

  it('should run setup-only module', async () => {
    const setupOnlyFlagPath = tmpdir.resolve('setup-only-executed.tmp');

    const { results } = await runTestWithGlobalHooks({
      globalSetupFile: 'setup-only.js',
      env: {
        SETUP_ONLY_FLAG_PATH: setupOnlyFlagPath,
        SETUP_FLAG_PATH: setupOnlyFlagPath
      }
    });

    assert.strictEqual(results.passed, 1);
    assert.strictEqual(results.failed, 1);
    assert.ok(fs.existsSync(setupOnlyFlagPath), 'Setup-only flag file should exist');
    const content = fs.readFileSync(setupOnlyFlagPath, 'utf8');
    assert.strictEqual(content, 'Setup-only was executed');
  });

  it('should run teardown-only module', async () => {
    const teardownOnlyFlagPath = tmpdir.resolve('teardown-only-executed.tmp');
    const setupFlagPath = tmpdir.resolve('setup-for-teardown-only.tmp');

    // Create a setup file for test-file.js to find
    fs.writeFileSync(setupFlagPath, 'Setup was executed');

    const { results } = await runTestWithGlobalHooks({
      globalSetupFile: 'teardown-only.js',
      env: {
        TEARDOWN_ONLY_FLAG_PATH: teardownOnlyFlagPath,
        SETUP_FLAG_PATH: setupFlagPath
      }
    });

    assert.strictEqual(results.passed, 2);
    assert.strictEqual(results.failed, 0);
    assert.ok(fs.existsSync(teardownOnlyFlagPath), 'Teardown-only flag file should exist');
    const content = fs.readFileSync(teardownOnlyFlagPath, 'utf8');
    assert.strictEqual(content, 'Teardown-only was executed');
  });

  it('should share context between setup and teardown', async () => {
    const contextFlagPath = tmpdir.resolve('context-shared.tmp');
    const setupFlagPath = tmpdir.resolve('setup-for-context.tmp');

    // Create a setup file for test-file.js to find
    fs.writeFileSync(setupFlagPath, 'Setup was executed');

    await runTestWithGlobalHooks({
      globalSetupFile: 'context-sharing.js',
      env: {
        CONTEXT_FLAG_PATH: contextFlagPath,
        SETUP_FLAG_PATH: setupFlagPath
      }
    });

    assert.ok(fs.existsSync(contextFlagPath), 'Context sharing flag file should exist');
    const contextData = JSON.parse(fs.readFileSync(contextFlagPath, 'utf8'));

    assert.strictEqual(typeof contextData.timestamp, 'number');
    assert.strictEqual(contextData.message, 'Hello from setup');
    assert.deepStrictEqual(contextData.complexData, { key: 'value', nested: { data: true } });
  });

  // TODO(pmarchini): This test is expected to fail because the teardown runs after the test stream closes.
  // We need to find a way to run the teardown before the stream closes.
  // This is related only to the usage of `run` function.
  it.todo('should handle async setup and teardown', async () => {
    const asyncFlagPath = tmpdir.resolve('async-executed.tmp');
    const setupFlagPath = tmpdir.resolve('setup-for-async.tmp');

    // Create a setup file for test-file.js to find
    fs.writeFileSync(setupFlagPath, 'Setup was executed');

    const { results } = await runTestWithGlobalHooks({
      globalSetupFile: 'async-setup-teardown.js',
      env: {
        ASYNC_FLAG_PATH: asyncFlagPath,
        SETUP_FLAG_PATH: setupFlagPath
      }
    });

    assert.strictEqual(results.passed, 2);
    assert.strictEqual(results.failed, 0);
    assert.ok(fs.existsSync(asyncFlagPath), 'Async flag file should exist');
    const content = fs.readFileSync(asyncFlagPath, 'utf8');
    assert.strictEqual(content, 'Setup part, Teardown part');
  });

  it('should run TypeScript globalSetup and globalTeardown functions', async () => {
    const setupFlagPath = tmpdir.resolve('setup-executed-ts.tmp');
    const teardownFlagPath = tmpdir.resolve('teardown-executed-ts.tmp');

    const { results } = await runTestWithGlobalHooks({
      globalSetupFile: 'basic-setup-teardown.ts',
      env: {
        SETUP_FLAG_PATH: setupFlagPath,
        TEARDOWN_FLAG_PATH: teardownFlagPath
      }
    });

    assert.strictEqual(results.passed, 2);
    assert.strictEqual(results.failed, 0);
    // After all tests complete, the teardown should have run
    assert.ok(fs.existsSync(teardownFlagPath), 'Teardown flag file should exist');
    const content = fs.readFileSync(teardownFlagPath, 'utf8');
    assert.strictEqual(content, 'Teardown was executed');

    // Setup flag should have been removed by teardown
    assert.ok(!fs.existsSync(setupFlagPath), 'Setup flag file should have been removed');
  });

  it('should run ESM globalSetup and globalTeardown functions', async () => {
    const setupFlagPath = tmpdir.resolve('setup-executed-esm.tmp');
    const teardownFlagPath = tmpdir.resolve('teardown-executed-esm.tmp');

    const { results } = await runTestWithGlobalHooks({
      globalSetupFile: 'basic-setup-teardown.mjs',
      env: {
        SETUP_FLAG_PATH: setupFlagPath,
        TEARDOWN_FLAG_PATH: teardownFlagPath
      }
    });

    assert.strictEqual(results.passed, 2);
    assert.strictEqual(results.failed, 0);
    // After all tests complete, the teardown should have run
    assert.ok(fs.existsSync(teardownFlagPath), 'Teardown flag file should exist');
    const content = fs.readFileSync(teardownFlagPath, 'utf8');
    assert.strictEqual(content, 'Teardown was executed');
    // Setup flag should have been removed by teardown
    assert.ok(!fs.existsSync(setupFlagPath), 'Setup flag file should have been removed');
  });

  it('should validate that globalSetupPath is a string', () => {
    [123, {}, [], true, false].forEach((invalidValue) => {
      assert.throws(() => {
        run({
          files: [path.join(testFixtures, 'test-file.js')],
          globalSetupPath: invalidValue
        });
      }, {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /The "options\.globalSetupPath" property must be of type string/
      });
    });
  });
});
