import { skip, spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { test } from 'node:test';

if (!process.config.variables.node_use_amaro) skip('Requires Amaro');

test('execute a TypeScript file with transformation enabled', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    '--no-warnings',
    fixtures.path('typescript/ts/transformation/test-enum.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('reconstruct error of a TypeScript file with transformation enabled and sourcemaps', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    '--no-warnings',
    fixtures.path('typescript/ts/transformation/test-enum-stacktrace.ts'),
  ]);

  assert.match(result.stderr, /test-enum-stacktrace\.ts:4:7/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('reconstruct error of a TypeScript file with transformation enabled without sourcemaps', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    '--no-enable-source-maps',
    '--no-warnings',
    fixtures.path('typescript/ts/transformation/test-enum-stacktrace.ts'),
  ]);
  assert.match(result.stderr, /test-enum-stacktrace\.ts:5:7/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('should not elide unused imports', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    '--no-warnings',
    fixtures.path('typescript/ts/transformation/test-unused-import.ts'),
  ]);
  assert.match(result.stderr, /ERR_UNSUPPORTED_DIR_IMPORT/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute a TypeScript file with namespace', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    '--no-warnings',
    fixtures.path('typescript/ts/transformation/test-namespace.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

// Decorators are currently ignored by transpilation
// and will be unusable until V8 adds support for them.
test('execute a TypeScript file with decorator', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    '--no-warnings',
    fixtures.path('typescript/ts/transformation/test-decorator.ts'),
  ]);

  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /Invalid or unexpected token/);
  assert.strictEqual(result.code, 1);
});

test('execute a TypeScript file with legacy-module', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    '--no-warnings',
    fixtures.path('typescript/ts/transformation/test-legacy-module.ts'),
  ]);

  assert.match(result.stderr, /ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX/);
  assert.match(result.stderr, /`module` keyword is not supported\. Use `namespace` instead/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute a TypeScript file with modern typescript syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    '--no-warnings',
    fixtures.path('typescript/ts/transformation/test-modern-typescript.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute a transpiled JavaScript file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--enable-source-maps',
    '--no-warnings',
    fixtures.path('typescript/ts/transformation/test-transformed-typescript.js'),
  ]);

  assert.match(result.stderr, /Stacktrace at line 28/);
  assert.match(result.stderr, /test-failing-arm64\.js:28:7/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute TypeScript file with import = require', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-transform-types',
    '--no-warnings',
    fixtures.path('typescript/cts/test-import-require.cts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});
