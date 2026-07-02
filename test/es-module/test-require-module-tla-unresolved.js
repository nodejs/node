'use strict';

// Tests that require(esm) throws for unresolved top-level-await.

const common = require('../common');
const assert = require('assert');

assert.throws(() => {
  require('../fixtures/es-modules/tla/unresolved.mjs');
}, (err) => {
  common.expectRequiredTLAError(err, [__filename]);
  return true;
});
