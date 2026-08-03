import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it, beforeEach, run } from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { once } from 'node:events';

const testFixtures = fixtures.path('test-runner', 'global-setup-teardown');
const runnerFixture = fixtures.path('test-runner', 'test-runner-global-hooks.mjs');

describe('require(\'node:test\').run with global hooks', { concurrency: false }, () => {
  beforeEach(() => {
    tmpdir.refresh();
  });

  async function runTestWithGlobalHooks({
    globalSetupFile,
    testFile = 'test-file.js',
    runnerEnv = {},
    isolation = 'process'
  }) {
    const testFilePath = path.join(testFixtures, testFile);
    const globalSetupPath = path.join(testFixtures, globalSetupFile);

    const child = spawn(
      process.execPath,
      [
        runnerFixture,
        '--file', testFilePath,
        '--globalSetup', globalSetupPath,
        '--isolation', isolation,
      ],
      {
        encoding: 'utf8',
        stdio: 'pipe',
        env: {
          ...runnerEnv,
          ...process.env,
          AVOID_PRINT_LOGS: 'true',
          NODE_OPTIONS: '--no-warnings',
        }
      }
    );

    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (data) => {
      stdout += data.toString();
    });
    child.stderr.on('data', (data) => {
      stderr += data.toString();
    });

    await once(child, 'exit');

    // Assert in order to print a detailed error message if the test fails
    assert.partialDeepStrictEqual(stderr, '');
    assert.match(stdout, /pass (\d+)/);
    assert.match(stdout, /fail (\d+)/);

    const results = {
      passed: parseInt((stdout.match(/pass (\d+)/) || [])[1] || '0', 10),
      failed: parseInt((stdout.match(/fail (\d+)/) || [])[1] || '0', 10)
    };

    return { results };
  }

  for (const isolation of ['none', 'process']) {
    describe(`with isolation : ${isolation}`, () => {
      it('should run globalSetup and globalTeardown functions', async () => {
        const setupFlagPath = tmpdir.resolve('setup-executed.tmp');
        const teardownFlagPath = tmpdir.resolve('teardown-executed.tmp');

        const { results } = await runTestWithGlobalHooks({
          globalSetupFile: 'basic-setup-teardown.js',
          runnerEnv: {
            SETUP_FLAG_PATH: setupFlagPath,
            TEARDOWN_FLAG_PATH: teardownFlagPath
          },
          isolation
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
          runnerEnv: {
            SETUP_ONLY_FLAG_PATH: setupOnlyFlagPath,
            SETUP_FLAG_PATH: setupOnlyFlagPath
          },
          isolation
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
          runnerEnv: {
            TEARDOWN_ONLY_FLAG_PATH: teardownOnlyFlagPath,
            SETUP_FLAG_PATH: setupFlagPath
          },
          isolation
        });

        assert.strictEqual(results.passed, 2);
        assert.strictEqual(results.failed, 0);
        assert.ok(fs.existsSync(teardownOnlyFlagPath), 'Teardown-only flag file should exist');
        const content = fs.readFileSync(teardownOnlyFlagPath, 'utf8');
        assert.strictEqual(content, 'Teardown-only was executed');
      });

      // TODO(pmarchini): We should be able to share context between setup and teardown
      it.todo('should share context between setup and teardown');

      it('should handle async setup and teardown', async () => {
        const asyncFlagPath = tmpdir.resolve('async-executed.tmp');
        const setupFlagPath = tmpdir.resolve('setup-for-async.tmp');

        // Create a setup file for test-file.js to find
        fs.writeFileSync(setupFlagPath, 'Setup was executed');

        const { results } = await runTestWithGlobalHooks({
          globalSetupFile: 'async-setup-teardown.js',
          runnerEnv: {
            ASYNC_FLAG_PATH: asyncFlagPath,
            SETUP_FLAG_PATH: setupFlagPath
          },
          isolation
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
          runnerEnv: {
            SETUP_FLAG_PATH: setupFlagPath,
            TEARDOWN_FLAG_PATH: teardownFlagPath
          },
          isolation
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
          runnerEnv: {
            SETUP_FLAG_PATH: setupFlagPath,
            TEARDOWN_FLAG_PATH: teardownFlagPath
          },
          isolation
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
    });
  }

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
