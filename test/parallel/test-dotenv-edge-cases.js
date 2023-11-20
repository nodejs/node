'use strict';

const common = require('../common');
const assert = require('node:assert');
const { describe, it } = require('node:test');

const validEnvFilePath = '../fixtures/dotenv/valid.env';
const nodeOptionsEnvFilePath = '../fixtures/dotenv/node-options.env';

describe('.env supports edge cases', () => {

  it('supports multiple declarations', async () => {
    // process.env.BASIC is equal to `basic` because the second .env file overrides it.
    const code = `
      const assert = require('assert');
      assert.strictEqual(process.env.BASIC, 'basic');
      assert.strictEqual(process.env.NODE_NO_WARNINGS, '1');
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ `--env-file=${nodeOptionsEnvFilePath}`, `--env-file=${validEnvFilePath}`, '--eval', code ],
      { cwd: __dirname },
    );
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should handle non-existent .env file', async () => {
    const code = `
      require('assert').strictEqual(1, 1)
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--env-file=.env', '--eval', code ],
      { cwd: __dirname },
    );
    assert.notStrictEqual(child.stderr.toString(), '');
    assert.strictEqual(child.code, 9);
  });

  it('should not override existing environment variables but introduce new vars', async () => {
    const code = `
      require('assert').strictEqual(process.env.BASIC, 'existing');
      require('assert').strictEqual(process.env.AFTER_LINE, 'after_line');
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ `--env-file=${validEnvFilePath}`, '--eval', code ],
      { cwd: __dirname, env: { ...process.env, BASIC: 'existing' } },
    );
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });
});
