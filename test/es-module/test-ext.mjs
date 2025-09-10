import { skip, spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { match, strictEqual } from 'node:assert';
import { test } from 'node:test';

if (!process.config.variables.node_use_amaro) skip('Requires Amaro');

test('test unknown value for --experimental-ext', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=foo',
    fixtures.path('ext/extensionless-ts'),
  ]);
  match(result.stderr, /--experimental-ext must be "js", "mjs", "cjs", "ts", "cts" or "mts"/);
});

test('check experimental warning is emitted', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--experimental-ext=ts',
    fixtures.path('ext/extensionless-ts'),
  ]);
  match(result.stderr, /--experimental-ext is an experimental feature/);
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('ovverides the extension in a extensionless ts file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=ts',
    fixtures.path('ext/extensionless-ts'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('ovverides the extension in a extensionless mts file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=ts',
    fixtures.path('ext/extensionless-mts'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('ovverides the extension in a extensionless cts file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=ts',
    fixtures.path('ext/extensionless-cts'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('run a .mts with cts content', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=cts',
    fixtures.path('ext/fake-cts.mts'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('run a .cts with mts content', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=mts',
    fixtures.path('ext/fake-mts.cts'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('run a .js with ts content', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=ts',
    fixtures.path('ext/actually-ts.js'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('run a random extension as a ts file', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=ts',
    fixtures.path('ext/actually-ts.foo'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('run a .cjs as mjs', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=mjs',
    fixtures.path('ext/actually-mjs.cjs'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('run a .cjs as js', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=js',
    fixtures.path('ext/actually-mjs.cjs'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

test('run a .mjs as cjs', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=cjs',
    fixtures.path('ext/actually-cjs.mjs'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});

// We try to require a .mjs file with cjs content
// and check the flag does not override its content since it should
// only work for the entry point
test('check the extension is not changed for non main files', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=cjs',
    fixtures.path('ext/mjs2cjs/b.mjs'),
  ]);
  match(result.stderr, /module is not defined in ES module scope/);
  strictEqual(result.stdout, '');
  strictEqual(result.code, 1);
});

// Try to import a .cjs with .mjs content
// Even if entry point is marked as mjs
test('should not import a cjs with mjs content', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=mjs',
    fixtures.path('ext/cjs2mjs/b.cjs'),
  ]);
  match(result.stderr, /SyntaxError: Cannot use import statement outside a module/);
  strictEqual(result.stdout, '');
  strictEqual(result.code, 1);
});

test('run a .json as ts', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--no-warnings',
    '--experimental-ext=ts',
    fixtures.path('ext/actually-ts.json'),
  ]);
  strictEqual(result.stderr, '');
  match(result.stdout, /Hello World!/);
  strictEqual(result.code, 0);
});
