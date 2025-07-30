'use strict';

require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { describe, it } = require('node:test');
const path = require('path');

// Test flag propagation to child test processes
// This validates that certain flags are/aren't propagated to child test processes
// based on the specification in the Node.js documentation
describe('test runner flag propagation', () => {
  const flagPropagationTests = [
    ['--experimental-config-file', 'node.config.json', ''],
    ['--experimental-default-config-file', '', false],
    ['--env-file', '.env', '.env'],
    ['--env-file-if-exists', '.env', '.env'],
    ['--test-concurrency', '2', '2'],
    // ['--test-force-exit', '', true], // <-- this test fails as is forces exit
    // ['--test-only', '', true], // <-- this needs to be investigated
    ['--test-timeout', '5000', '5000'],
    ['--test-coverage-branches', '100', '100'],
    ['--test-coverage-functions', '100', '100'],
    ['--test-coverage-lines', '100', '100'],
    ['--experimental-test-coverage', '', false],
    ['--test-coverage-exclude', 'test/**', 'test/**'],
    ['--test-coverage-include', 'src/**', 'src/**'],
    ['--test-update-snapshots', '', true],
    ['--import', './index.js', './index.js'],
    ['--require', './index.js', './index.js'],
  ];

  // Path to the static fixture
  const fixtureDir = fixtures.path('test-runner', 'flag-propagation');
  const runner = path.join(fixtureDir, 'runner.mjs');

  for (const [flagName, testValue, expectedValue] of flagPropagationTests) {
    const testDescription = `should propagate ${flagName} to child tests as expected`;

    it(testDescription, () => {
      const args = [
        '--test-reporter=tap',
        '--no-warnings',
        '--expose-internals',
        // We need to pass the flag that will be propagated to the child test
        testValue ? `${flagName}=${testValue}` : flagName,
        // Use the runner fixture
        runner,
        // Pass parameters to the fixture
        `--flag=${flagName}`,
        `--expected=${expectedValue}`,
        `--description="${testDescription}"`,
      ].filter(Boolean);

      const child = spawnSync(
        process.execPath,
        args,
        {
          cwd: fixtureDir,
        },
      );

      assert.strictEqual(child.status, 0, `Flag propagation test failed for ${flagName}.`);
      const stdout = child.stdout.toString();
      assert.match(stdout, /tests 1/, `Test should execute for ${flagName}`);
      assert.match(stdout, /pass 1/, `Test should pass for ${flagName} propagation check`);
    });
  }
});
