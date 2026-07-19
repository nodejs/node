'use strict';

// Tests that require(esm) throws for top-level-await in inner graphs.

const common = require('../common');
const assert = require('assert');

assert.throws(() => {
  require('../fixtures/es-modules/tla/parent.mjs');
}, (err) => {
  common.expectRequiredTLAError(err, [__filename]);
  return true;
});
