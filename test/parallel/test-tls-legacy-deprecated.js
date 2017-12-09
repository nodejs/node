// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

common.expectWarning(
  'DeprecationWarning',
  'tls.createSecurePair() is deprecated. Please use tls.TLSSocket instead.'
);

assert.doesNotThrow(() => tls.createSecurePair());
