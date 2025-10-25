import { skip, spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { test } from 'node:test';

if (!process.config.variables.node_use_amaro) skip('Requires Amaro');

test('expect failure of a .mts file with CommonJS syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/mts/test-mts-but-commonjs-syntax.mts'),
  ]);

  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /require is not defined in ES module scope, you can use import instead/);
  assert.strictEqual(result.code, 1);
});

test('execute an .mts file importing an .mts file', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/mts/test-import-module.mts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute an .mts file importing a .ts file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/mts/test-import-ts-file.mts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute an .mts file importing a .cts file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/mts/test-import-commonjs.mts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('execute an .mts file from node_modules', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/mts/test-mts-node_modules.mts'),
  ]);

  assert.match(result.stderr, /ERR_UNSUPPORTED_NODE_MODULES_TYPE_STRIPPING/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute a .cts file from node_modules', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/mts/test-cts-node_modules.mts'),
  ]);

  assert.match(result.stderr, /ERR_UNSUPPORTED_NODE_MODULES_TYPE_STRIPPING/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute a .ts file from node_modules', async () => {
  const result = await spawnPromisified(process.execPath, [
    fixtures.path('typescript/mts/test-ts-node_modules.mts'),
  ]);

  assert.match(result.stderr, /ERR_UNSUPPORTED_NODE_MODULES_TYPE_STRIPPING/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('execute an empty .ts file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-empty-file.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 0);
});

test('execute .ts file importing a module', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/ts/test-import-fs.ts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.strictEqual(result.stdout, 'Hello, TypeScript!\n');
  assert.strictEqual(result.code, 0);
});

test('mts -> import cts -> require mts', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    fixtures.path('typescript/mts/issue-59963/a.mts'),
  ]);

  assert.strictEqual(result.stderr, '');
  assert.strictEqual(result.stdout, 'Hello from c.mts\n');
  assert.strictEqual(result.code, 0);
});
