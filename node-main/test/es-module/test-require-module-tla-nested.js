'use strict';

// Tests that require(esm) throws for top-level-await in inner graphs.

const common = require('../common');
const assert = require('assert');

assert.throws(() => {
  require('../fixtures/es-modules/tla/parent.mjs');
}, (err) => {
  common.expectRequiredTLAError(err);
  assert.match(err.message, /From .*test-require-module-tla-nested\.js/);
  assert.match(err.message, /Requiring .*parent\.mjs/);
  return true;
});
