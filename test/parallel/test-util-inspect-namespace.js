// Flags: --experimental-vm-modules
'use strict';
const common = require('../common');
const assert = require('assert');

const { SourceTextModule } = require('vm');
const { inspect } = require('util');

(async () => {
  const m = new SourceTextModule('export const a = 1; export var b = 2');
  await m.link(() => 0);
  assert.strictEqual(
    inspect(m.namespace),
    '[Module: null prototype] { a: <uninitialized>, b: undefined }');
  await m.evaluate();
  assert.strictEqual(
    inspect(m.namespace),
    '[Module: null prototype] { a: 1, b: 2 }'
  );
})().then(common.mustCall());
