import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('Loader hooks', { concurrency: true }, () => {
  it('are called with all expected arguments', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/hooks-input.mjs'),
      fixtures.path('/es-modules/json-modules.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);

    const lines = stdout.split('\n');
    assert.match(lines[0], /{"url":"file:\/\/\/.*\/json-modules\.mjs","format":"test","shortCircuit":true}/);
    assert.match(lines[1], /{"source":{"type":"Buffer","data":\[.*\]},"format":"module","shortCircuit":true}/);
    assert.match(lines[2], /{"url":"file:\/\/\/.*\/experimental\.json","format":"test","shortCircuit":true}/);
    assert.match(lines[3], /{"source":{"type":"Buffer","data":\[.*\]},"format":"json","shortCircuit":true}/);
  });

  it('should be able to handle never resolving dynamic imports (ESM entry point)', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
      fixtures.path('es-module-loaders/never-settling-resolve-step/never-resolve.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^should be output\r?\n$/);
    assert.strictEqual(code, 13);
    assert.strictEqual(signal, null);
  });

  it('should be able to handle never resolving dynamic imports (CJS entry point)', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
      fixtures.path('es-module-loaders/never-settling-resolve-step/never-resolve.cjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^should be output\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should be able to handle never loading dynamic imports (ESM entry point)', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
      fixtures.path('es-module-loaders/never-settling-resolve-step/never-load.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^should be output\r?\n$/);
    assert.strictEqual(code, 13);
    assert.strictEqual(signal, null);
  });

  it('should be able to handle never loading dynamic imports (CJS entry point)', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
      fixtures.path('es-module-loaders/never-settling-resolve-step/never-load.cjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^should be output\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should be able to handle race of never settling dynamic imports (ESM entry point)', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
      fixtures.path('es-module-loaders/never-settling-resolve-step/race.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^true\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should be able to handle race of never settling dynamic imports (CJS entry point)', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
      fixtures.path('es-module-loaders/never-settling-resolve-step/race.cjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^true\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});
