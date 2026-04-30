'use strict';

const { spawnPromisified } = require('../common');
const { test } = require('node:test');

test('node:validator cannot be accessed without the node: scheme', (t) => {
  t.assert.throws(() => {
    require('validator');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: /Cannot find module 'validator'/,
  });
});

test('node:validator is disabled by --no-experimental-validator', async (t) => {
  const { stdout, stderr, code, signal } = await spawnPromisified(
    process.execPath,
    [
      '--no-experimental-validator',
      '-e',
      'require("node:validator")',
    ],
  );

  t.assert.strictEqual(stdout, '');
  t.assert.match(stderr, /No such built-in module: node:validator/);
  t.assert.notStrictEqual(code, 0);
  t.assert.strictEqual(signal, null);
});
