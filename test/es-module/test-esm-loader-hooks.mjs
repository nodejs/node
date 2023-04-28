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

  describe('should handle never-settling hooks in ESM files', { concurrency: true }, () => {
    it('top-level await of a never-settling resolve', async () => {
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

    it('top-level await of a never-settling load', async () => {
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


    it('top-level await of a race of never-settling hooks', async () => {
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

    it('import.meta.resolve of a never-settling resolve', async () => {
      const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
        '--no-warnings',
        '--experimental-import-meta-resolve',
        '--experimental-loader',
        fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
        fixtures.path('es-module-loaders/never-settling-resolve-step/import.meta.never-resolve.mjs'),
      ]);

      assert.strictEqual(stderr, '');
      assert.match(stdout, /^should be output\r?\n$/);
      assert.strictEqual(code, 13);
      assert.strictEqual(signal, null);
    });
  });

  describe('should handle never-settling hooks in CJS files', { concurrency: true }, () => {
    it('never-settling resolve', async () => {
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


    it('never-settling load', async () => {
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

    it('race of never-settling hooks', async () => {
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

  it('should work without worker permission', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-permission',
      '--allow-fs-read',
      '*',
      '--experimental-loader',
      fixtures.fileURL('empty.js'),
      fixtures.path('es-modules/esm-top-level-await.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^1\r?\n2\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should allow loader hooks to spawn workers when allowed by the CLI flags', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-permission',
      '--allow-worker',
      '--allow-fs-read',
      '*',
      '--experimental-loader',
      `data:text/javascript,import{Worker}from"worker_threads";new Worker(${encodeURIComponent(JSON.stringify(fixtures.path('empty.js')))}).unref()`,
      fixtures.path('es-modules/esm-top-level-await.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^1\r?\n2\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should not allow loader hooks to spawn workers if restricted by the CLI flags', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-permission',
      '--allow-fs-read',
      '*',
      '--experimental-loader',
      `data:text/javascript,import{Worker}from"worker_threads";new Worker(${encodeURIComponent(JSON.stringify(fixtures.path('empty.js')))}).unref()`,
      fixtures.path('es-modules/esm-top-level-await.mjs'),
    ]);

    assert.match(stderr, /code: 'ERR_ACCESS_DENIED'/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should not leak internals or expose import.meta.resolve', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-import-meta-resolve',
      '--experimental-loader',
      fixtures.fileURL('es-module-loaders/loader-edge-cases.mjs'),
      fixtures.path('empty.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should be fine to call `process.exit` from a custom async hook', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-import-meta-resolve',
      '--experimental-loader',
      'data:text/javascript,export function load(a,b,next){if(a==="data:exit")process.exit(42);return next(a,b)}',
      '--input-type=module',
      '--eval',
      'import "data:exit"',
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 42);
    assert.strictEqual(signal, null);
  });

  it('should be fine to call `process.exit` from a custom sync hook', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-import-meta-resolve',
      '--experimental-loader',
      'data:text/javascript,export function resolve(a,b,next){if(a==="exit:")process.exit(42);return next(a,b)}',
      '--input-type=module',
      '--eval',
      'import "data:text/javascript,import.meta.resolve(%22exit:%22)"',
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 42);
    assert.strictEqual(signal, null);
  });

  it('should be fine to call `process.exit` from the loader thread top-level', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,process.exit(42)',
      fixtures.path('empty.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 42);
    assert.strictEqual(signal, null);
  });
});
