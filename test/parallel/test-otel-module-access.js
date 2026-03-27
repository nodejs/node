'use strict';
const common = require('../common');
const { spawnPromisified } = common;
const assert = require('node:assert');
const { describe, it } = require('node:test');

describe('node:otel module access', () => {
  it('cannot be accessed without the node: scheme', async () => {
    const { code, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-otel',
      '-e',
      'require("otel")',
    ]);
    assert.notStrictEqual(code, 0);
    assert.match(stderr, /Cannot find module 'otel'/);
  });

  it('cannot be accessed without --experimental-otel flag', async () => {
    const { code, stderr } = await spawnPromisified(process.execPath, [
      '-e',
      'require("node:otel")',
    ]);
    assert.notStrictEqual(code, 0);
    assert.match(stderr, /No such built-in module: node:otel/);
  });

  it('can be accessed with --experimental-otel flag', async () => {
    const { code, stdout } = await spawnPromisified(process.execPath, [
      '--experimental-otel',
      '-e',
      'const otel = require("node:otel"); console.log(typeof otel.start)',
    ]);
    assert.strictEqual(code, 0);
    assert.match(stdout, /function/);
  });

  it('can be accessed when NODE_OTEL_ENDPOINT is set', async () => {
    const { code, stdout } = await spawnPromisified(process.execPath, [
      '-e',
      'const otel = require("node:otel"); console.log(typeof otel.start)',
    ], {
      env: {
        ...process.env,
        // Use a dummy endpoint that will fail silently.
        NODE_OTEL_ENDPOINT: 'http://127.0.0.1:1',
      },
    });
    assert.strictEqual(code, 0);
    assert.match(stdout, /function/);
  });

  it('emits experimental warning', async () => {
    const { code, stderr } = await spawnPromisified(process.execPath, [
      '--experimental-otel',
      '-e',
      'require("node:otel")',
    ]);
    assert.strictEqual(code, 0);
    assert.match(stderr, /ExperimentalWarning: node:otel/);
  });
});
