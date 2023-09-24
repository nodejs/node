import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it } from 'node:test';
import assert from 'node:assert';

describe('--experimental-type=module should affect the behavior of files without extensions', { concurrency: true }, () => {
  it('should run an extensionless module that is outside of any package scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/noext-esm'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'executed\n');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should run an extensionless module within a "type": "module" scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-type-module/noext-esm'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'executed\n');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should import an extensionless module that is outside of any scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/imports-noext.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'executed\n');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should import an extensionless module within a "type": "module" scope', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-type-module/imports-noext.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'executed\n');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});

describe('--experimental-type=module should not affect the behavior of files with unknown extensions', { concurrency: true }, () => {
  it('should error on an entry point with an unknown extension', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-type-module/extension.unknown'),
    ]);

    assert.match(stderr, /ERR_UNKNOWN_FILE_EXTENSION/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should error on an import with an unknown extension', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-type=module',
      fixtures.path('es-modules/package-type-module/imports-unknownext.mjs'),
    ]);

    assert.match(stderr, /ERR_UNKNOWN_FILE_EXTENSION/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });
});
