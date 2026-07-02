'use strict';

// Tests that require(esm) throws for resolved top-level-await.

const common = require('../common');
const assert = require('assert');

assert.throws(() => {
  require('../fixtures/es-modules/tla/resolved.mjs');
}, (err) => {
  common.expectRequiredTLAError(err, [__filename]);
  return true;
});
