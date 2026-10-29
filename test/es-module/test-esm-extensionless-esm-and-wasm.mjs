// Flags: --experimental-wasm-modules
import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it } from 'node:test';
import assert from 'node:assert';

describe('extensionless ES modules within a "type": "module" package scope', {
  concurrency: !process.env.TEST_PARALLEL,
}, () => {
  it('should run as the entry point', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      fixtures.path('es-modules/package-type-module/noext-esm'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'executed\n');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should be importable', async () => {
    const { default: defaultExport } =
              await import(fixtures.fileURL('es-modules/package-type-module/noext-esm'));
    assert.strictEqual(defaultExport, 'module');
  });

  it('should be importable from a module scope under node_modules', async () => {
    const { default: defaultExport } =
      await import(fixtures.fileURL(
        'es-modules/package-type-module/node_modules/dep-with-package-json-type-module/noext-esm'));
    assert.strictEqual(defaultExport, 'module');
  });
});
describe('extensionless Wasm modules within a "type": "module" package scope', {
  concurrency: !process.env.TEST_PARALLEL,
}, () => {
  it('should run as the entry point', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-wasm-modules',
      '--no-warnings',
      fixtures.path('es-modules/package-type-module/noext-wasm'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, 'executed\n');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should be importable', async () => {
    const { add } = await import(fixtures.fileURL('es-modules/package-type-module/noext-wasm'));
    assert.strictEqual(add(1, 2), 3);
  });

  it('should be importable from a module scope under node_modules', async () => {
    const { add } = await import(fixtures.fileURL(
      'es-modules/package-type-module/node_modules/dep-with-package-json-type-module/noext-wasm'));
    assert.strictEqual(add(1, 2), 3);
  });
});

describe('extensionless ES modules within no package scope', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should run as the entry point', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      fixtures.path('es-modules/noext-esm'),
    ]);

    assert.strictEqual(stdout, 'executed\n');
    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should run on import', async () => {
    await import(fixtures.fileURL('es-modules/noext-esm'));
  });
});

describe('extensionless Wasm within no package scope', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should error as the entry point', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-wasm-modules',
      '--no-warnings',
      fixtures.path('es-modules/noext-wasm'),
    ]);

    assert.match(stderr, /SyntaxError/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should run on import', async () => {
    await import(fixtures.fileURL('es-modules/noext-wasm'));
  });
});
