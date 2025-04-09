import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it, beforeEach } from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import { spawn } from 'node:child_process';
import tmpdir from '../common/tmpdir.js';
import { once } from 'node:events';
import { join } from 'node:path';

const testFixtures = fixtures.path('test-runner');

async function runTest(
  {
    isolation,
    globalSetupFile,
    testFile = 'test-file.js',
    env = {},
    additionalFlags = []
  }
) {
  const testFilePath = join(testFixtures, 'global-setup-teardown', testFile);
  const globalSetupPath = join(testFixtures, 'global-setup-teardown', globalSetupFile);

  const child = spawn(
    process.execPath,
    [
      '--test',
      '--test-isolation=' + isolation,
      '--test-reporter=spec',
      '--test-global-setup=' + globalSetupPath,
      [...additionalFlags],
      testFilePath,
    ],
    {
      encoding: 'utf8',
      stdio: 'pipe',
      env: {
        ...process.env,
        ...env
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

  return { stdout, stderr };
}

['none', 'process'].forEach((isolation) => {
  describe(`test runner global hooks with isolation=${isolation}`, { concurrency: false }, () => {
    beforeEach(() => {
      tmpdir.refresh();
    });

    it('should run globalSetup and globalTeardown functions', async () => {
      const setupFlagPath = tmpdir.resolve('setup-executed.tmp');
      const teardownFlagPath = tmpdir.resolve('teardown-executed.tmp');

      const { stdout, stderr } = await runTest({
        isolation,
        globalSetupFile: 'basic-setup-teardown.js',
        env: {
          SETUP_FLAG_PATH: setupFlagPath,
          TEARDOWN_FLAG_PATH: teardownFlagPath
        }
      });

      assert.match(stdout, /pass 2/);
      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Global setup executed/);
      assert.match(stdout, /Global teardown executed/);
      assert.match(stdout, /Data from setup: data from setup/);
      assert.strictEqual(stderr.length, 0);

      // After all tests complete, the teardown should have run
      assert.ok(fs.existsSync(teardownFlagPath), 'Teardown flag file should exist');
      const content = fs.readFileSync(teardownFlagPath, 'utf8');
      assert.strictEqual(content, 'Teardown was executed');

      // Setup flag should have been removed by teardown
      assert.ok(!fs.existsSync(setupFlagPath), 'Setup flag file should have been removed');
    });

    it('should run setup-only module', async () => {
      const setupOnlyFlagPath = tmpdir.resolve('setup-only-executed.tmp');

      const { stdout } = await runTest({
        isolation,
        globalSetupFile: 'setup-only.js',
        env: {
          SETUP_ONLY_FLAG_PATH: setupOnlyFlagPath,
          SETUP_FLAG_PATH: setupOnlyFlagPath
        }
      });

      assert.match(stdout, /Setup-only module executed/);

      assert.ok(fs.existsSync(setupOnlyFlagPath), 'Setup-only flag file should exist');
      const content = fs.readFileSync(setupOnlyFlagPath, 'utf8');
      assert.strictEqual(content, 'Setup-only was executed');
    });

    it('should run teardown-only module', async () => {
      const teardownOnlyFlagPath = tmpdir.resolve('teardown-only-executed.tmp');
      const setupFlagPath = tmpdir.resolve('setup-for-teardown-only.tmp');

      // Create a setup file for test-file.js to find
      fs.writeFileSync(setupFlagPath, 'Setup was executed');

      const { stdout } = await runTest({
        isolation,
        globalSetupFile: 'teardown-only.js',
        env: {
          TEARDOWN_ONLY_FLAG_PATH: teardownOnlyFlagPath,
          SETUP_FLAG_PATH: setupFlagPath
        }
      });

      assert.match(stdout, /pass 2/);
      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Teardown-only module executed/);

      assert.ok(fs.existsSync(teardownOnlyFlagPath), 'Teardown-only flag file should exist');
      const content = fs.readFileSync(teardownOnlyFlagPath, 'utf8');
      assert.strictEqual(content, 'Teardown-only was executed');
    });

    it('should share context between setup and teardown', async () => {
      const contextFlagPath = tmpdir.resolve('context-shared.tmp');
      const setupFlagPath = tmpdir.resolve('setup-for-context.tmp');

      // Create a setup file for test-file.js to find
      fs.writeFileSync(setupFlagPath, 'Setup was executed');

      await runTest({
        isolation,
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

    it('should handle async setup and teardown', async () => {
      const asyncFlagPath = tmpdir.resolve('async-executed.tmp');
      const setupFlagPath = tmpdir.resolve('setup-for-async.tmp');

      // Create a setup file for test-file.js to find
      fs.writeFileSync(setupFlagPath, 'Setup was executed');

      const { stdout } = await runTest({
        isolation,
        globalSetupFile: 'async-setup-teardown.js',
        env: {
          ASYNC_FLAG_PATH: asyncFlagPath,
          SETUP_FLAG_PATH: setupFlagPath
        }
      });

      assert.match(stdout, /pass 2/);
      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Async setup starting/);
      assert.match(stdout, /Async setup completed/);
      assert.match(stdout, /Async teardown starting/);
      assert.match(stdout, /Async teardown completed/);

      assert.ok(fs.existsSync(asyncFlagPath), 'Async flag file should exist');
      const content = fs.readFileSync(asyncFlagPath, 'utf8');
      assert.strictEqual(content, 'Setup part, Teardown part');
    });

    it('should handle error in setup', async () => {
      const setupFlagPath = tmpdir.resolve('setup-for-error.tmp');

      const { stdout, stderr } = await runTest({
        isolation,
        globalSetupFile: 'error-in-setup.js',
        env: {
          SETUP_FLAG_PATH: setupFlagPath
        }
      });

      // Verify that the error is reported properly
      const errorReported = stderr.includes('Deliberate error in global setup');
      assert.ok(errorReported, 'Should report the error from global setup');

      // Verify the teardown wasn't executed
      assert.ok(!stdout.includes('Teardown should not run if setup fails'),
                'Teardown should not run after setup fails');
    });

    it('should run TypeScript globalSetup and globalTeardown functions', async () => {
      const setupFlagPath = tmpdir.resolve('setup-executed-ts.tmp');
      const teardownFlagPath = tmpdir.resolve('teardown-executed-ts.tmp');

      const { stdout, stderr } = await runTest({
        isolation,
        globalSetupFile: 'basic-setup-teardown.ts',
        env: {
          SETUP_FLAG_PATH: setupFlagPath,
          TEARDOWN_FLAG_PATH: teardownFlagPath
        },
        additionalFlags: ['--no-warnings']
      });

      assert.match(stdout, /pass 2/);
      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Global setup executed/);
      assert.match(stdout, /Global teardown executed/);
      assert.match(stdout, /Data from setup: data from setup/);
      assert.strictEqual(stderr.length, 0);

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

      const { stdout, stderr } = await runTest({
        isolation,
        globalSetupFile: 'basic-setup-teardown.mjs',
        env: {
          SETUP_FLAG_PATH: setupFlagPath,
          TEARDOWN_FLAG_PATH: teardownFlagPath
        }
      });

      assert.match(stdout, /pass 2/);
      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Global setup executed/);
      assert.match(stdout, /Global teardown executed/);
      assert.match(stdout, /Data from setup: data from setup/);
      assert.strictEqual(stderr.length, 0);

      // After all tests complete, the teardown should have run
      assert.ok(fs.existsSync(teardownFlagPath), 'Teardown flag file should exist');
      const content = fs.readFileSync(teardownFlagPath, 'utf8');
      assert.strictEqual(content, 'Teardown was executed');

      // Setup flag should have been removed by teardown
      assert.ok(!fs.existsSync(setupFlagPath), 'Setup flag file should have been removed');
    });
  });
});
