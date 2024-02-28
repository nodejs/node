'use strict';

const common = require('../common');
const fixtures = require('../../test/common/fixtures');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const basicValidEnvFilePath = fixtures.path('dotenv/basic-valid.env');
const validEnvFilePath = fixtures.path('dotenv/valid.env');
const missingEnvFile = fixtures.path('dotenv/non-existent-file.env');

describe('process.loadEnvFile()', () => {

  it('supports passing path', async () => {
    const code = `
      process.loadEnvFile(${JSON.stringify(validEnvFilePath)});
      const assert = require('assert');
      assert.strictEqual(process.env.BASIC, 'basic');
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--eval', code ],
      { cwd: __dirname },
    );
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('supports not-passing a path', async () => {
    // Uses `../fixtures/dotenv/.env` file.
    const code = `
      process.loadEnvFile();
      const assert = require('assert');
      assert.strictEqual(process.env.BASIC, 'basic');
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--eval', code ],
      { cwd: fixtures.path('dotenv/') },
    );
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should throw when file does not exist', async () => {
    assert.throws(() => {
      process.loadEnvFile(missingEnvFile);
    }, { code: 'ENOENT' });
  });

  it('should throw when `.env` does not exist', async () => {
    assert.throws(() => {
      process.loadEnvFile();
    }, { code: 'ENOENT' });
  });

  it('should check for permissions', async () => {
    const code = `
      process.loadEnvFile(${JSON.stringify(missingEnvFile)});
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--eval', code, '--experimental-permission' ],
      { cwd: __dirname },
    );
    assert.match(child.stderr, /Error: Access to this API has been restricted/);
    assert.match(child.stderr, /code: 'ERR_ACCESS_DENIED'/);
    assert.match(child.stderr, /permission: 'FileSystemRead'/);
    if (!common.isWindows) {
      assert(child.stderr.includes(`resource: '${JSON.stringify(missingEnvFile).replaceAll('"', '')}'`));
    }
    assert.strictEqual(child.code, 1);
  });

  it('loadEnvFile does not mutate --env-file output', async () => {
    const code = `
      process.loadEnvFile(${JSON.stringify(basicValidEnvFilePath)});
      require('assert')(process.env.BASIC === 'basic');
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ `--env-file=${validEnvFilePath}`, '--eval', code ],
      { cwd: __dirname },
    );
    assert.strictEqual(child.stdout, '');
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });
});
