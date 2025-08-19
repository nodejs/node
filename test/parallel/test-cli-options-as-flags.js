'use strict';

const {
  spawnPromisified,
} = require('../common');
const fixtures = require('../common/fixtures');
const { strictEqual } = require('node:assert');
const { describe, it } = require('node:test');
const path = require('node:path');

const fixtureFile = fixtures.path(path.join('options-as-flags', 'fixture.cjs'));
const configFile = fixtures.path(path.join('options-as-flags', 'test-config.json'));
const envFile = fixtures.path(path.join('options-as-flags', '.test.env'));

describe('getOptionsAsFlagsFromBinding', () => {
  it('should extract flags from command line arguments', async () => {
    const result = await spawnPromisified(process.execPath, [
      '--no-warnings',
      '--expose-internals',
      '--stack-trace-limit=512',
      fixtureFile,
    ]);

    strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    strictEqual(flags.includes('--no-warnings'), true);
    strictEqual(flags.includes('--stack-trace-limit=512'), true);
  });

  it('should extract flags from NODE_OPTIONS environment variable', async () => {
    const result = await spawnPromisified(process.execPath, [
      '--no-warnings',
      '--expose-internals',
      fixtureFile,
    ], {
      env: {
        ...process.env,
        NODE_OPTIONS: '--stack-trace-limit=4096'
      }
    });

    strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    // Should contain the flag from NODE_OPTIONS
    strictEqual(flags.includes('--stack-trace-limit=4096'), true);
    // Should also contain command line flags
    strictEqual(flags.includes('--no-warnings'), true);
  });

  it('should extract flags from config file', async () => {
    const result = await spawnPromisified(process.execPath, [
      '--no-warnings',
      '--expose-internals',
      '--experimental-config-file',
      configFile,
      fixtureFile,
    ]);

    strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    // Should contain flags from config file
    strictEqual(flags.includes('--experimental-transform-types'), true);
    strictEqual(flags.includes('--max-http-header-size=8192'), true);
    strictEqual(flags.includes('--test-isolation=none'), true);
    // Should also contain command line flags
    strictEqual(flags.includes('--no-warnings'), true);
  });

  it('should extract flags from config file and command line', async () => {
    const result = await spawnPromisified(process.execPath, [
      '--no-warnings',
      '--expose-internals',
      '--stack-trace-limit=512',
      '--experimental-config-file',
      configFile,
      fixtureFile,
    ]);

    strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    // Should contain flags from command line arguments
    strictEqual(flags.includes('--no-warnings'), true);
    strictEqual(flags.includes('--stack-trace-limit=512'), true);

    // Should contain flags from config file
    strictEqual(flags.includes('--experimental-transform-types'), true);
    strictEqual(flags.includes('--max-http-header-size=8192'), true);
    strictEqual(flags.includes('--test-isolation=none'), true);
  });

  it('should extract flags from .env file', async () => {
    const result = await spawnPromisified(process.execPath, [
      '--no-warnings',
      '--expose-internals',
      `--env-file=${envFile}`,
      fixtureFile,
    ]);

    strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    // Should contain flags from .env file (NODE_OPTIONS)
    strictEqual(flags.includes('--v8-pool-size=8'), true);
    // Should also contain command line flags
    strictEqual(flags.includes('--no-warnings'), true);
  });
});
