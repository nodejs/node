// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const assert = require('assert');
const tls = require('tls');

common.expectWarning(
  'DeprecationWarning',
  'tls.createSecurePair() is deprecated. Please use tls.Socket instead.'
);

assert.doesNotThrow(() => tls.createSecurePair());
