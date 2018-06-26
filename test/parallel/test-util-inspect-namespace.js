'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');
const assert = require('assert');

common.crashOnUnhandledRejection();

const { Module } = require('vm');
const { inspect } = require('util');

(async () => {
  const m = new Module('export const a = 1; export var b = 2');
  await m.link(() => 0);
  m.instantiate();
  assert.strictEqual(
    inspect(m.namespace),
    '[Module] { a: <uninitialized>, b: undefined }');
  await m.evaluate();
  assert.strictEqual(inspect(m.namespace), '[Module] { a: 1, b: 2 }');
})();
