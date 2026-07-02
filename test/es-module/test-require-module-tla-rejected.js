'use strict';

// Tests that require(esm) throws for rejected top-level await.

const common = require('../common');
const assert = require('assert');

assert.throws(() => {
  require('../fixtures/es-modules/tla/rejected.mjs');
}, (err) => {
  common.expectRequiredTLAError(err, [__filename]);
  return true;
});
