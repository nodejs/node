import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('--entry-url', { concurrency: true }, () => {
  it('should reject loading absolute path that contains %', async () => {
    const { code, signal, stderr, stdout } = await spawnPromisified(
      execPath,
      [
        '--experimental-entry-url',
        fixtures.path('es-modules/test-esm-double-encoding-native%20.mjs'),
      ]
    );

    assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should support loading absolute URLs', async () => {
    const { code, signal, stderr, stdout } = await spawnPromisified(
      execPath,
      [
        '--experimental-entry-url',
        fixtures.fileURL('printA.js'),
      ]
    );

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^A\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should support loading relative URLs', async () => {
    const { code, signal, stderr, stdout } = await spawnPromisified(
      execPath,
      [
        '--experimental-entry-url',
        './printA.js?key=value',
      ],
      {
        cwd: fixtures.fileURL('./'),
      }
    );

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^A\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should reject loading relative URLs without trailing `./`', async () => {
    const { code, signal, stderr, stdout } = await spawnPromisified(
      execPath,
      [
        '--experimental-entry-url',
        'printA.js?key=value',
      ],
      {
        cwd: fixtures.fileURL('./'),
      }
    );

    assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should support loading `data:` URLs', async () => {
    const { code, signal, stderr, stdout } = await spawnPromisified(
      execPath,
      [
        '--experimental-entry-url',
        'data:text/javascript,console.log(0)',
      ],
    );

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^0\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

});
