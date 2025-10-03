'use strict';

// Tests that require(esm) throws for resolved top-level-await.

const common = require('../common');
const assert = require('assert');

assert.throws(() => {
  require('../fixtures/es-modules/tla/resolved.mjs');
}, (err) => {
  common.expectRequiredTLAError(err);
  assert.match(err.message, /From .*test-require-module-tla-resolved\.js/);
  assert.match(err.message, /Requiring .*resolved\.mjs/);
  return true;
});
