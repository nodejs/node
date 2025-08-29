'use strict';

const common = require('../common');
const assert = require('node:assert');
const path = require('node:path');
const { describe, it } = require('node:test');
const { parseEnv, loadEnvFile } = require('node:util');

const validEnvFilePath = '../fixtures/dotenv/valid.env';
const multilineEnvFilePath = '../fixtures/dotenv/multiline.env';
const linesWithOnlySpacesEnvFilePath = '../fixtures/dotenv/lines-with-only-spaces.env';
const eofWithoutValueEnvFilePath = '../fixtures/dotenv/eof-without-value.env';
const nodeOptionsEnvFilePath = '../fixtures/dotenv/node-options.env';
const noFinalNewlineEnvFilePath = '../fixtures/dotenv/no-final-newline.env';
const noFinalNewlineSingleQuotesEnvFilePath = '../fixtures/dotenv/no-final-newline-single-quotes.env';

describe('.env supports edge cases', () => {
  it('supports multiple declarations, including optional ones', async () => {
    const code = `
      assert.strictEqual(process.env.BASIC, 'basic');
      assert.strictEqual(process.env.NODE_NO_WARNINGS, '1');
    `.trim();
    const children = await Promise.all(Array.from({ length: 4 }, (_, i) =>
      common.spawnPromisified(
        process.execPath,
        [
          // Bitwise AND to create all 4 possible combinations:
          // i & 0b01 is truthy when i has value 0bx1 (i.e. 0b01 (1) and 0b11 (3)), falsy otherwise.
          // i & 0b10 is truthy when i has value 0b1x (i.e. 0b10 (2) and 0b11 (3)), falsy otherwise.
          `${i & 0b01 ? '--env-file' : '--env-file-if-exists'}=${nodeOptionsEnvFilePath}`,
          `${i & 0b10 ? '--env-file' : '--env-file-if-exists'}=${validEnvFilePath}`,
          '--eval', code,
        ],
        { cwd: __dirname },
      )));
    assert.deepStrictEqual(children, Array.from({ length: 4 }, () => ({
      code: 0,
      signal: null,
      stdout: '',
      stderr: '',
    })));
  });

  it('supports absolute paths', async () => {
    const code = `
      assert.strictEqual(process.env.BASIC, 'basic');
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ `--env-file=${path.resolve(__dirname, validEnvFilePath)}`, '--eval', code ],
    );
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('supports a space instead of \'=\' for the flag ', async () => {
    const code = `
      assert.strictEqual(process.env.BASIC, 'basic');
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--env-file', validEnvFilePath, '--eval', code ],
      { cwd: __dirname },
    );
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should handle non-existent .env file', async () => {
    const code = `
      assert.strictEqual(1, 1)
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ '--env-file=.env', '--eval', code ],
      { cwd: __dirname },
    );
    assert.notStrictEqual(child.stderr, '');
    assert.strictEqual(child.code, 9);
  });

  it('should handle non-existent optional .env file', async () => {
    const code = `
      assert.strictEqual(1,1);
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      ['--env-file-if-exists=.env', '--eval', code],
      { cwd: __dirname },
    );
    assert.notStrictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should not override existing environment variables but introduce new vars', async () => {
    const code = `
      assert.strictEqual(process.env.BASIC, 'existing');
      assert.strictEqual(process.env.AFTER_LINE, 'after_line');
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ `--env-file=${validEnvFilePath}`, '--eval', code ],
      { cwd: __dirname, env: { ...process.env, BASIC: 'existing' } },
    );
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should handle multiline quoted values', async () => {
    // Ref: https://github.com/nodejs/node/issues/52248
    const obj = loadEnvFile(path.resolve(__dirname, multilineEnvFilePath));
    assert.match(obj.JWT_PUBLIC_KEY, /-----BEGIN PUBLIC KEY-----\n[\s\S]*\n-----END PUBLIC KEY-----*/);
  });

  it('should handle empty value without a newline at the EOF', async () => {
    // Ref: https://github.com/nodejs/node/issues/52466
    const obj = loadEnvFile(path.resolve(__dirname, eofWithoutValueEnvFilePath));
    assert.strictEqual(obj.BASIC, 'value');
    assert.strictEqual(obj.EMPTY, '');
  });

  it('should handle lines that come after lines with only spaces (and tabs)', async () => {
    // Ref: https://github.com/nodejs/node/issues/56686
    const obj = loadEnvFile(path.resolve(__dirname, linesWithOnlySpacesEnvFilePath));
    assert.strictEqual(obj.EMPTY_LINE, 'value after an empty line');
    assert.strictEqual(obj.SPACES_LINE, 'value after a line with just some spaces');
    assert.strictEqual(obj.TABS_LINE, 'value after a line with just some tabs');
    assert.strictEqual(obj.SPACES_TABS_LINE, 'value after a line with just some spaces and tabs');
  });

  it('should handle when --env-file is passed along with --', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      [
        '--eval', `assert.strictEqual(process.env.BASIC, undefined);`,
        '--', '--env-file', validEnvFilePath,
      ],
      { cwd: __dirname },
    );
    assert.strictEqual(child.stdout, '');
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should handle file without a final newline', async () => {
    const code = `
      assert.strictEqual(process.env.BASIC, 'basic');
    `.trim();
    const child = await common.spawnPromisified(
      process.execPath,
      [ `--env-file=${path.resolve(__dirname, noFinalNewlineEnvFilePath)}`, '--eval', code ],
    );

    const SingleQuotesChild = await common.spawnPromisified(
      process.execPath,
      [ `--env-file=${path.resolve(__dirname, noFinalNewlineSingleQuotesEnvFilePath)}`, '--eval', code ],
    );

    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
    assert.strictEqual(SingleQuotesChild.stderr, '');
    assert.strictEqual(SingleQuotesChild.code, 0);
  });

  it('should reject invalid env file flag', async () => {
    const child = await common.spawnPromisified(
      process.execPath,
      ['--env-file-ABCD', validEnvFilePath],
      { cwd: __dirname },
    );

    assert.strictEqual(child.stdout, '');
    assert.strictEqual(child.code, 9);
    assert.match(child.stderr, /bad option: --env-file-ABCD/);
  });

  it('should handle invalid multiline syntax', () => {
    const result = parseEnv([
      'foo',
      '',
      'bar',
      'baz=whatever',
      'VALID_AFTER_INVALID=test',
      'multiple_invalid',
      'lines_without_equals',
      'ANOTHER_VALID=value',
    ].join('\n'));

    assert.deepStrictEqual(result, {
      baz: 'whatever',
      VALID_AFTER_INVALID: 'test',
      ANOTHER_VALID: 'value',
    });
  });

  it('should handle trimming of keys and values correctly', () => {
    const result = parseEnv([
      '   KEY_WITH_SPACES_BEFORE=   value_with_spaces_before_and_after   ',
      'KEY_WITH_TABS_BEFORE\t=\tvalue_with_tabs_before_and_after\t',
      'KEY_WITH_SPACES_AND_TABS\t = \t value_with_spaces_and_tabs \t',
      '   KEY_WITH_SPACES_ONLY   =value',
      'KEY_WITH_NO_VALUE=',
      'KEY_WITH_SPACES_AFTER=   value   ',
      'KEY_WITH_SPACES_AND_COMMENT=value   # this is a comment',
      'KEY_WITH_ONLY_COMMENT=# this is a comment',
      'KEY_WITH_EXPORT=export value',
      '   export   KEY_WITH_EXPORT_AND_SPACES   =   value   ',
    ].join('\n'));

    assert.deepStrictEqual(result, {
      KEY_WITH_SPACES_BEFORE: 'value_with_spaces_before_and_after',
      KEY_WITH_TABS_BEFORE: 'value_with_tabs_before_and_after',
      KEY_WITH_SPACES_AND_TABS: 'value_with_spaces_and_tabs',
      KEY_WITH_SPACES_ONLY: 'value',
      KEY_WITH_NO_VALUE: '',
      KEY_WITH_ONLY_COMMENT: '',
      KEY_WITH_SPACES_AFTER: 'value',
      KEY_WITH_SPACES_AND_COMMENT: 'value',
      KEY_WITH_EXPORT: 'export value',
      KEY_WITH_EXPORT_AND_SPACES: 'value',
    });
  });

  it('should handle a comment in a valid value', () => {
    const result = parseEnv([
      'KEY_WITH_COMMENT_IN_VALUE="value # this is a comment"',
    ].join('\n'));

    assert.deepStrictEqual(result, {
      KEY_WITH_COMMENT_IN_VALUE: 'value # this is a comment',
    });
  });
});
