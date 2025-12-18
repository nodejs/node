'use strict';

// Tests that require(esm) throws for unresolved top-level-await.

const common = require('../common');
const assert = require('assert');

assert.throws(() => {
  require('../fixtures/es-modules/tla/unresolved.mjs');
}, (err) => {
  common.expectRequiredTLAError(err);
  assert.match(err.message, /From .*test-require-module-tla-unresolved\.js/);
  assert.match(err.message, /Requiring .*unresolved\.mjs/);
  return true;
});
