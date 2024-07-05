// Flags: --experimental-require-module
'use strict';
const common = require('../common');
const mod = require('../fixtures/es-modules/export-es-module.mjs');

// If an ESM already defines __esModule to be something else,
// require(esm) should allow the user override.
common.expectRequiredModule(
  mod,
  { default: { hello: 'world' }, __esModule: 'test' },
  false,
);
