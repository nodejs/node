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
    testFiles = ['test-file.js'],
    env = {},
    additionalFlags = [],
    runnerEnabled = true,
  }
) {
  const globalSetupPath = join(testFixtures, 'global-setup-teardown', globalSetupFile);

  const args = [];

  if (runnerEnabled) {
    args.push('--test');
  }

  if (isolation) {
    args.push(`--test-isolation=${isolation}`);
  }

  args.push(
    '--test-reporter=spec',
    `--test-global-setup=${globalSetupPath}`
  );

  if (additionalFlags.length > 0) {
    args.push(...additionalFlags);
  }

  const testFilePaths = testFiles.map((file) => join(testFixtures, 'global-setup-teardown', file));
  args.push(...testFilePaths);

  const child = spawn(
    process.execPath,
    args,
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

[
  {
    isolation: 'none',
    runnerEnabled: true
  },
  {
    isolation: 'process',
    runnerEnabled: true
  },
  {
    isolation: undefined,
    runnerEnabled: false
  },
].forEach((testCase) => {
  const { isolation, runnerEnabled } = testCase;
  describe(`test runner global hooks with isolation=${isolation} and --test: ${runnerEnabled}`, { concurrency: false }, () => {
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
        },
        runnerEnabled
      });

      assert.match(stdout, /pass 2/);
      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Global setup executed/);
      assert.match(stdout, /Global teardown executed/);
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
        },
        runnerEnabled
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
        },
        runnerEnabled
      });

      assert.match(stdout, /pass 2/);
      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Teardown-only module executed/);

      assert.ok(fs.existsSync(teardownOnlyFlagPath), 'Teardown-only flag file should exist');
      const content = fs.readFileSync(teardownOnlyFlagPath, 'utf8');
      assert.strictEqual(content, 'Teardown-only was executed');
    });

    // Create a file in globalSetup and delete it in globalTeardown
    // two test files should both verify that the file exists
    // This works as the globalTeardown removes the setupFlag
    it('should run globalTeardown only after all tests are done in case of more than one test file', async () => {
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
        },
        runnerEnabled,
        testFiles: ['test-file.js', 'another-test-file.js']
      });

      if (runnerEnabled) {
        assert.match(stdout, /pass 3/);
      } else {
        assert.match(stdout, /pass 2/);
      }

      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Teardown-only module executed/);

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

      const { stdout } = await runTest({
        isolation,
        globalSetupFile: 'async-setup-teardown.js',
        env: {
          ASYNC_FLAG_PATH: asyncFlagPath,
          SETUP_FLAG_PATH: setupFlagPath
        },
        runnerEnabled
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
        },
        runnerEnabled
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
        additionalFlags: ['--no-warnings'],
        runnerEnabled
      });

      assert.match(stdout, /pass 2/);
      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Global setup executed/);
      assert.match(stdout, /Global teardown executed/);
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
        },
        runnerEnabled
      });

      assert.match(stdout, /pass 2/);
      assert.match(stdout, /fail 0/);
      assert.match(stdout, /Global setup executed/);
      assert.match(stdout, /Global teardown executed/);
      assert.strictEqual(stderr.length, 0);

      // After all tests complete, the teardown should have run
      assert.ok(fs.existsSync(teardownFlagPath), 'Teardown flag file should exist');
      const content = fs.readFileSync(teardownFlagPath, 'utf8');
      assert.strictEqual(content, 'Teardown was executed');

      // Setup flag should have been removed by teardown
      assert.ok(!fs.existsSync(setupFlagPath), 'Setup flag file should have been removed');
    });

    it('should run globalSetup only once for run', async () => {
      const setupFlagPath = tmpdir.resolve('setup-executed-once.tmp');
      const teardownFlagPath = tmpdir.resolve('teardown-executed-once.tmp');

      const { stdout } = await runTest({
        isolation,
        globalSetupFile: 'basic-setup-teardown.js',
        env: {
          SETUP_FLAG_PATH: setupFlagPath,
          TEARDOWN_FLAG_PATH: teardownFlagPath
        },
        runnerEnabled
      });

      const GlobalSetupOccurrences = (stdout.match(/Global setup executed/g) || []).length;

      // Global setup should run only once
      assert.strictEqual(GlobalSetupOccurrences, 1);
    });

    it('should run globalSetup and globalTeardown only once for run', async () => {
      const setupFlagPath = tmpdir.resolve('setup-executed-once.tmp');
      const teardownFlagPath = tmpdir.resolve('teardown-executed-once.tmp');

      const { stdout } = await runTest({
        isolation,
        globalSetupFile: 'basic-setup-teardown.js',
        env: {
          SETUP_FLAG_PATH: setupFlagPath,
          TEARDOWN_FLAG_PATH: teardownFlagPath
        },
        runnerEnabled
      });

      const GlobalTeardownOccurrences = (stdout.match(/Global teardown executed/g) || []).length;

      // Global teardown should run only once
      assert.strictEqual(GlobalTeardownOccurrences, 1);
    });

    it('should run globalSetup and globalTeardown only once for run with multiple test files',
       {
         skip: !runnerEnabled ? 'Skipping test as --test is not enabled' : false
       },
       async () => {
         const setupFlagPath = tmpdir.resolve('setup-executed-once.tmp');
         const teardownFlagPath = tmpdir.resolve('teardown-executed-once.tmp');
         const testFiles = ['test-file.js', 'another-test-file.js'];
         const { stdout } = await runTest({
           isolation,
           globalSetupFile: 'basic-setup-teardown.js',
           env: {
             SETUP_FLAG_PATH: setupFlagPath,
             TEARDOWN_FLAG_PATH: teardownFlagPath
           },
           runnerEnabled,
           testFiles
         });
         const GlobalSetupOccurrences = (stdout.match(/Global setup executed/g) || []).length;
         const GlobalTeardownOccurrences = (stdout.match(/Global teardown executed/g) || []).length;

         assert.strictEqual(GlobalSetupOccurrences, 1);
         assert.strictEqual(GlobalTeardownOccurrences, 1);

         assert.match(stdout, /pass 3/);
         assert.match(stdout, /fail 0/);
       }
    );

    describe('interop with --require and --import', () => {
      const cjsPath = join(testFixtures, 'global-setup-teardown', 'required-module.cjs');
      const esmpFile = fixtures.fileURL('test-runner', 'global-setup-teardown', 'imported-module.mjs');

      // The difference in behavior is due to how --require and --import are handled by
      // the main entry point versus the test runner entry point.
      // When isolation is 'none', both --require and --import are handled by the test runner.
      const shouldRequireAfterSetup = runnerEnabled && isolation === 'none';
      const shouldImportAfterSetup = runnerEnabled;

      it(`should run required module ${shouldRequireAfterSetup ? 'after' : 'before'} globalSetup`, async () => {
        const setupFlagPath = tmpdir.resolve('setup-for-required.tmp');
        const teardownFlagPath = tmpdir.resolve('teardown-for-required.tmp');

        fs.writeFileSync(setupFlagPath, '');

        const { stdout } = await runTest({
          isolation,
          globalSetupFile: 'basic-setup-teardown.js',
          requirePath: './required-module.js',
          env: {
            SETUP_FLAG_PATH: setupFlagPath,
            TEARDOWN_FLAG_PATH: teardownFlagPath
          },
          additionalFlags: [
            `--require=${cjsPath}`,
          ],
          runnerEnabled
        });

        assert.match(stdout, /pass 2/);
        assert.match(stdout, /fail 0/);
        assert.match(stdout, /Required module executed/);
        assert.match(stdout, /Global setup executed/);
        assert.match(stdout, /Global teardown executed/);

        const requiredExecutedPosition = stdout.indexOf('Required module executed');
        const globalSetupExecutedPosition = stdout.indexOf('Global setup executed');

        assert.ok(
          shouldRequireAfterSetup ?
            requiredExecutedPosition > globalSetupExecutedPosition :
            requiredExecutedPosition < globalSetupExecutedPosition,
          `Required module should have been executed ${shouldRequireAfterSetup ? 'after' : 'before'} global setup`
        );

        assert.ok(fs.existsSync(teardownFlagPath), 'Teardown flag file should exist');
        const content = fs.readFileSync(teardownFlagPath, 'utf8');
        assert.strictEqual(content, 'Teardown was executed');
        assert.ok(!fs.existsSync(setupFlagPath), 'Setup flag file should have been removed');
      });

      it(`should run imported module ${shouldImportAfterSetup ? 'after' : 'before'} globalSetup`, async () => {
        const setupFlagPath = tmpdir.resolve('setup-for-imported.tmp');
        const teardownFlagPath = tmpdir.resolve('teardown-for-imported.tmp');

        fs.writeFileSync(setupFlagPath, 'non-empty');

        const { stdout } = await runTest({
          isolation,
          globalSetupFile: 'basic-setup-teardown.mjs',
          importPath: './imported-module.js',
          env: {
            SETUP_FLAG_PATH: setupFlagPath,
            TEARDOWN_FLAG_PATH: teardownFlagPath
          },
          additionalFlags: [
            `--import=${esmpFile}`,
          ],
          runnerEnabled
        });

        assert.match(stdout, /pass 2/);
        assert.match(stdout, /fail 0/);
        assert.match(stdout, /Imported module executed/);
        assert.match(stdout, /Global setup executed/);
        assert.match(stdout, /Global teardown executed/);

        const importedExecutedPosition = stdout.indexOf('Imported module executed');
        const globalSetupExecutedPosition = stdout.indexOf('Global setup executed');

        assert.ok(
          shouldImportAfterSetup ?
            importedExecutedPosition > globalSetupExecutedPosition :
            importedExecutedPosition < globalSetupExecutedPosition,
          `Imported module should have been executed ${shouldImportAfterSetup ? 'after' : 'before'} global setup`
        );

        assert.ok(fs.existsSync(teardownFlagPath), 'Teardown flag file should exist');
        const content = fs.readFileSync(teardownFlagPath, 'utf8');
        assert.strictEqual(content, 'Teardown was executed');
        assert.ok(!fs.existsSync(setupFlagPath), 'Setup flag file should have been removed');
      });

      it('should execute globalSetup and globalTeardown correctly with imported module containing tests', async () => {
        const setupFlagPath = tmpdir.resolve('setup-executed.tmp');
        const teardownFlagPath = tmpdir.resolve('teardown-executed.tmp');
        const importedModuleWithTestFile = fixtures.fileURL(
          'test-runner',
          'global-setup-teardown',
          'imported-module-with-test.mjs'
        );
        // Create a setup file for test-file.js to find
        fs.writeFileSync(setupFlagPath, 'non-empty');

        const { stdout } = await runTest({
          isolation,
          globalSetupFile: 'basic-setup-teardown.js',
          env: {
            SETUP_FLAG_PATH: setupFlagPath,
            TEARDOWN_FLAG_PATH: teardownFlagPath
          },
          additionalFlags: [
            `--import=${importedModuleWithTestFile}`,
          ],
          runnerEnabled
        });

        assert.match(stdout, /Global setup executed/);
        assert.match(stdout, /Imported module Ok/);
        assert.match(stdout, /Imported module Fail/);
        assert.match(stdout, /verify setup was executed/);
        assert.match(stdout, /another simple test/);
        assert.match(stdout, /Global teardown executed/);
        assert.match(stdout, /tests 4/);
        assert.match(stdout, /suites 0/);
        assert.match(stdout, /pass 3/);
        assert.match(stdout, /fail 1/);
        assert.match(stdout, /cancelled 0/);
        assert.match(stdout, /skipped 0/);
        assert.match(stdout, /todo 0/);

        // After all tests complete, the teardown should have run
        assert.ok(fs.existsSync(teardownFlagPath), 'Teardown flag file should exist');
        const content = fs.readFileSync(teardownFlagPath, 'utf8');
        assert.strictEqual(content, 'Teardown was executed');
        // Setup flag should have been removed by teardown
        assert.ok(!fs.existsSync(setupFlagPath), 'Setup flag file should have been removed');
      });
    });
  });
});
