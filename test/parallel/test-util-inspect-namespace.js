'use strict';

const common = require('../common');

common.requireFlags('--experimental-vm-modules');

const assert = require('assert');

const { SourceTextModule } = require('vm');
const { inspect } = require('util');

(async () => {
  const m = new SourceTextModule('export const a = 1; export var b = 2');
  await m.link(() => 0);
  m.instantiate();
  assert.strictEqual(
    inspect(m.namespace),
    '[Module] { a: <uninitialized>, b: undefined }');
  await m.evaluate();
  assert.strictEqual(inspect(m.namespace), '[Module] { a: 1, b: 2 }');
})();
