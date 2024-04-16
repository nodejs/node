// Flags: --experimental-require-module
'use strict';

// This tests that previously synchronously loaded submodule can still
// be loaded by dynamic import().

const common = require('../common');
const assert = require('assert');

(async () => {
  const required = require('../fixtures/es-modules/require-and-import/load.cjs');
  const imported = await import('../fixtures/es-modules/require-and-import/load.mjs');
  assert.deepStrictEqual({ ...required }, { ...imported });
})().then(common.mustCall());
