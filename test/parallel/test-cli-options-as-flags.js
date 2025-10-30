'use strict';

const {
  spawnPromisified,
} = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
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

    assert.strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    assert.strictEqual(flags.includes('--no-warnings'), true);
    assert.strictEqual(flags.includes('--stack-trace-limit=512'), true);
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

    assert.strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    // Should contain the flag from NODE_OPTIONS
    assert.strictEqual(flags.includes('--stack-trace-limit=4096'), true);
    // Should also contain command line flags
    assert.strictEqual(flags.includes('--no-warnings'), true);
  });

  it('should extract flags from config file', async () => {
    const result = await spawnPromisified(process.execPath, [
      '--no-warnings',
      '--expose-internals',
      '--experimental-config-file',
      configFile,
      fixtureFile,
    ]);

    assert.strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    // Should contain flags from config file
    assert.strictEqual(flags.includes('--experimental-transform-types'), true);
    assert.strictEqual(flags.includes('--max-http-header-size=8192'), true);
    assert.strictEqual(flags.includes('--test-isolation=none'), true);
    // Should also contain command line flags
    assert.strictEqual(flags.includes('--no-warnings'), true);
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

    assert.strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    // Should contain flags from command line arguments
    assert.strictEqual(flags.includes('--no-warnings'), true);
    assert.strictEqual(flags.includes('--stack-trace-limit=512'), true);

    // Should contain flags from config file
    assert.strictEqual(flags.includes('--experimental-transform-types'), true);
    assert.strictEqual(flags.includes('--max-http-header-size=8192'), true);
    assert.strictEqual(flags.includes('--test-isolation=none'), true);
  });

  it('should extract flags from .env file', async () => {
    const result = await spawnPromisified(process.execPath, [
      '--no-warnings',
      '--expose-internals',
      `--env-file=${envFile}`,
      fixtureFile,
    ]);

    assert.strictEqual(result.code, 0);
    const flags = JSON.parse(result.stdout.trim());

    // Should contain flags from .env file (NODE_OPTIONS)
    assert.strictEqual(flags.includes('--v8-pool-size=8'), true);
    // Should also contain command line flags
    assert.strictEqual(flags.includes('--no-warnings'), true);
  });
});
