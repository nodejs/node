'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { describe, it } = require('node:test');
const { loadEnvFile } = require('node:util');
const { join } = require('node:path');
const { isMainThread } = require('worker_threads');

const validEnvFilePath = fixtures.path('dotenv/valid.env');
const missingEnvFile = fixtures.path('dir%20with unusual"chars \'åß∂ƒ©∆¬…`/non-existent-file.env');

describe('util.loadEnvFile()', () => {
  it('supports passing path', async () => {
    const obj = loadEnvFile(validEnvFilePath);
    assert(obj);
    assert.strictEqual(typeof obj, 'object');
  });

  it('supports not-passing a path', async () => {
    // Uses `../fixtures/dotenv/.env` file.
    const code = `
      const { loadEnvFile } = require('node:util');
      const obj = loadEnvFile();
      const assert = require('assert');
      assert(obj);
      assert.strictEqual(typeof obj, 'object');
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
      loadEnvFile(missingEnvFile);
    }, { code: 'ENOENT', syscall: 'open', path: missingEnvFile });
  });

  // The whole chdir flow here is to address a case where a developer
  // has a .env in the worktree which is probably in the global .gitignore.
  // In that case this test would fail. To avoid confusion, chdir to lib will
  // make this way less likely to happen. Probably a temporary directory would
  // be the best choice but given how edge this case is this is fine.
  it('should throw when `.env` does not exist', async () => {
    const originalCwd = process.cwd();

    try {
      if (isMainThread) {
        process.chdir(join(originalCwd, 'lib'));
      }

      assert.throws(() => {
        loadEnvFile();
      }, { code: 'ENOENT', syscall: 'open', path: '.env' });
    } finally {
      if (isMainThread) {
        process.chdir(originalCwd);
      }
    }
  });

  it('should check for permissions', async () => {
    const code = `
      require('node:util').loadEnvFile(${JSON.stringify(missingEnvFile)});
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--eval', code, '--permission' ],
      { cwd: __dirname },
    );
    assert.match(child.stderr, /Error: Access to this API has been restricted/);
    assert.match(child.stderr, /code: 'ERR_ACCESS_DENIED'/);
    assert.match(child.stderr, /permission: 'FileSystemRead'/);
    if (!common.isWindows) {
      const resource = /^\s+resource: (['"])(.+)\1$/m.exec(child.stderr);
      assert(resource);
      assert.strictEqual(resource[2], resource[1] === "'" ?
        missingEnvFile.replaceAll("'", "\\'") :
        JSON.stringify(missingEnvFile).slice(1, -1));
    }
    assert.strictEqual(child.code, 1);
  });
});
