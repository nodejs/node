'use strict';

require('../common');
const fixtures = require('../common/fixtures.js');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');
const { spawnSync } = require('node:child_process');
const { describe, it } = require('node:test');
const path = require('node:path');

const fixtureDir = fixtures.path('test-runner', 'flag-propagation');
const runner = path.join(fixtureDir, 'runner.mjs');

describe('test runner flag propagation', () => {
  describe('via command line', () => {
    const flagPropagationTests = [
      ['--experimental-config-file', 'node.config.json', ''],
      ['--experimental-default-config-file', '', false],
      ['--env-file', '.env', '.env'],
      ['--env-file-if-exists', '.env', '.env'],
      ['--test-concurrency', '2', '2'],
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

  describe('via config file', () => {
    const configFilePropagationTests = [
      ['--test-concurrency', 2, 2, 'testRunner'],
      ['--test-timeout', 5000, 5000, 'testRunner'],
      ['--test-coverage-branches', 100, 100, 'testRunner'],
      ['--test-coverage-functions', 100, 100, 'testRunner'],
      ['--test-coverage-lines', 100, 100, 'testRunner'],
      ['--experimental-test-coverage', true, false, 'testRunner'],
      ['--test-coverage-exclude', 'test/**', 'test/**', 'testRunner'],
      ['--test-coverage-include', 'src/**', 'src/**', 'testRunner'],
      ['--test-update-snapshots', true, true, 'testRunner'],
      ['--test-concurrency', 3, 3, 'testRunner'],
      ['--test-timeout', 2500, 2500, 'testRunner'],
      ['--test-coverage-branches', 90, 90, 'testRunner'],
      ['--test-coverage-functions', 85, 85, 'testRunner'],
    ];

    for (const [flagName, configValue, expectedValue, namespace] of configFilePropagationTests) {
      const testDescription = `should propagate ${flagName} from config file (${namespace}) to child tests`;

      it(testDescription, () => {
        tmpdir.refresh();

        // Create a temporary config file
        const configFile = path.join(tmpdir.path, 'test-config.json');
        const configContent = {
          [namespace]: {
            [flagName.replace('--', '')]: configValue
          }
        };

        fs.writeFileSync(configFile, JSON.stringify(configContent, null, 2));

        const args = [
          '--test-reporter=tap',
          '--no-warnings',
          '--expose-internals',
          `--experimental-config-file=${configFile}`,
          runner,
          `--flag=${flagName}`,
          `--expected=${expectedValue}`,
          `--description="${testDescription}"`,
        ];

        const child = spawnSync(
          process.execPath,
          args,
          {
            cwd: fixtureDir,
          },
        );

        assert.strictEqual(child.status, 0, `Config file propagation test failed for ${flagName}.`);
        const stdout = child.stdout.toString();
        assert.match(stdout, /tests 1/, `Test should execute for config file ${flagName}`);
        assert.match(stdout, /pass 1/, `Test should pass for config file ${flagName} propagation check`);
      });
    }
  });
});
