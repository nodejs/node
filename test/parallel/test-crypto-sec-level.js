// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const secLevel = require('internal/crypto/util').getOpenSSLSecLevel();
assert.ok(secLevel >= 0 && secLevel <= 5);
