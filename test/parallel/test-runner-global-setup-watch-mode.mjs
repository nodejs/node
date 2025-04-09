import '../common/index.mjs';
import { beforeEach, describe, it } from 'node:test';
import * as fixtures from '../common/fixtures.mjs';
import { once } from 'node:events';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { writeFileSync, existsSync, readFileSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import { join } from 'node:path';

const testFixtures = fixtures.path('test-runner');

describe('test runner watch mode with global setup hooks', () => {
  beforeEach(() => {
    tmpdir.refresh();
  });

  for (const isolation of ['none', 'process']) {
    it(`should run global setup/teardown hooks with each test run in watch mode (isolation: ${isolation})`, async () => {
      const testContent = `
          const test = require('node:test');
          
          test('test with global hooks', (t) => {
            t.assert.ok('test passed');
          });
        `;

      const globalSetupFileFixture = join(testFixtures, 'global-setup-teardown', 'basic-setup-teardown.mjs');
      const testFilePath = tmpdir.resolve('test-with-hooks.js');
      const setupFlagPath = tmpdir.resolve('setup-executed-watch.tmp');
      const teardownFlagPath = tmpdir.resolve('teardown-executed-watch.tmp');

      writeFileSync(testFilePath, testContent);

      const ran1 = Promise.withResolvers();
      const ran2 = Promise.withResolvers();

      const child = spawn(process.execPath,
                          [
                            '--watch',
                            '--test',
                            '--test-reporter=spec',
                            `--test-isolation=${isolation}`,
                            '--test-global-setup=' + globalSetupFileFixture,
                            testFilePath,
                          ],
                          {
                            encoding: 'utf8',
                            stdio: 'pipe',
                            cwd: tmpdir.path,
                            env: {
                              ...process.env,
                              SETUP_FLAG_PATH: setupFlagPath,
                              TEARDOWN_FLAG_PATH: teardownFlagPath
                            }
                          });

      let stdout = '';
      let currentRun = '';
      const runs = [];

      child.stdout.on('data', (data) => {
        stdout += data.toString();
        currentRun += data.toString();
        const testRuns = stdout.match(/duration_ms\s\d+/g);
        if (testRuns?.length >= 1) ran1.resolve();
        if (testRuns?.length >= 2) ran2.resolve();
      });

      await ran1.promise;
      runs.push(currentRun);
      currentRun = '';

      assert.ok(existsSync(setupFlagPath), 'Setup flag file should exist');
      assert.ok(!existsSync(teardownFlagPath), 'Teardown flag file should not exist');

      // Modify test file to trigger watch
      writeFileSync(testFilePath, testContent + `
          test('another test', (t) => {
            t.assert.ok('another test passed');
          });
        `);

      await ran2.promise;
      runs.push(currentRun);

      currentRun = '';
      child.kill();
      await once(child, 'exit');


      // Verify the teardown file was updated
      const secondTeardownContent = readFileSync(teardownFlagPath, 'utf8');
      assert.strictEqual(secondTeardownContent, 'Teardown was executed');

      assert.match(runs[0], /Global setup executed/);
      assert.match(runs[0], /tests 1/);
      assert.match(runs[0], /pass 1/);
      assert.match(runs[0], /fail 0/);

      assert.doesNotMatch(runs[1], /Global setup executed/);
      assert.doesNotMatch(runs[1], /Global teardown executed/);
      assert.match(runs[1], /tests 2/);
      assert.match(runs[1], /pass 2/);
      assert.match(runs[1], /fail 0/);

      // Verify stdout after killing the child
      assert.match(currentRun, /Global teardown executed/);
      assert.match(currentRun, /Data from setup: data from setup/);
    });
  }
});
