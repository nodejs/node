// Flags: --experimental-require-module
'use strict';

const common = require('../common');
const assert = require('assert');

(async () => {
  await import('../fixtures/es-modules/tla/resolved.mjs');
  assert.throws(() => {
    require('../fixtures/es-modules/tla/resolved.mjs');
  }, (err) => {
    common.expectRequiredTLAError(err, [__filename]);
    return true;
  });
})().then(common.mustCall());
