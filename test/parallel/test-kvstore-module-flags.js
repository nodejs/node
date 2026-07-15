// Tests for node:kvstore experimental module access-control:
// 1. Schemeless require('kvstore') must throw MODULE_NOT_FOUND.
// 2. --no-experimental-kvstore disables require('node:kvstore').
'use strict';

const { spawnPromisified, skipIfKVStoreMissing } = require('../common');
skipIfKVStoreMissing();

const { suite, test } = require('node:test');

suite('accessing the node:kvstore module', () => {
  test('cannot be accessed without the node: scheme', (t) => {
    t.assert.throws(() => {
      require('kvstore');
    }, {
      code: 'MODULE_NOT_FOUND',
      message: /Cannot find module 'kvstore'/,
    });
  });

  test('can be disabled with --no-experimental-kvstore flag', async (t) => {
    const {
      stdout,
      stderr,
      code,
      signal,
    } = await spawnPromisified(process.execPath, [
      '--no-experimental-kvstore',
      '-e',
      'require("node:kvstore")',
    ]);

    t.assert.strictEqual(stdout, '');
    t.assert.match(stderr, /No such built-in module: node:kvstore/);
    t.assert.notStrictEqual(code, 0);
    t.assert.strictEqual(signal, null);
  });
});
