import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import os from 'node:os';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('globalPreload hook', () => {
  it('should not emit deprecation warning when initialize is supplied', async () => {
    const { stderr } = await spawnPromisified(execPath, [
      '--experimental-loader',
      'data:text/javascript,export function globalPreload(){}export function initialize(){}',
      fixtures.path('empty.js'),
    ]);

    assert.doesNotMatch(stderr, /`globalPreload` is an experimental feature/);
  });

  it('should handle globalPreload returning undefined', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,export function globalPreload(){}',
      fixtures.path('empty.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should handle loading node:test', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,export function globalPreload(){return `getBuiltin("node:test")()`}',
      fixtures.path('empty.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /\n# pass 1\r?\n/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should handle loading node:os with node: prefix', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,export function globalPreload(){return `console.log(getBuiltin("node:os").arch())`}',
      fixtures.path('empty.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout.trim(), os.arch());
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  // `os` is used here because it's simple and not mocked (the builtin module otherwise doesn't matter).
  it('should handle loading builtin module without node: prefix', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,export function globalPreload(){return `console.log(getBuiltin("os").arch())`}',
      fixtures.path('empty.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout.trim(), os.arch());
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should throw when loading node:test without node: prefix', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,export function globalPreload(){return `getBuiltin("test")()`}',
      fixtures.path('empty.js'),
    ]);

    assert.match(stderr, /ERR_UNKNOWN_BUILTIN_MODULE/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should register globals set from globalPreload', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,export function globalPreload(){return "this.myGlobal=4"}',
      '--print', 'myGlobal',
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout.trim(), '4');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should log console.log calls returned from globalPreload', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,export function globalPreload(){return `console.log("Hello from globalPreload")`}',
      fixtures.path('empty.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout.trim(), 'Hello from globalPreload');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should crash if globalPreload returns code that throws', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      'data:text/javascript,export function globalPreload(){return `throw new Error("error from globalPreload")`}',
      fixtures.path('empty.js'),
    ]);

    assert.match(stderr, /error from globalPreload/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should have a `this` value that is not bound to the loader instance', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      `data:text/javascript,export ${function globalPreload() {
        if (this != null) {
          throw new Error('hook function must not be bound to ESMLoader instance');
        }
      }}`,
      fixtures.path('empty.js'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});
