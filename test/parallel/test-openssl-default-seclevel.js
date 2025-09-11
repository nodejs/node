// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (process.config.variables.node_shared_openssl)
  common.skip('not applicable when dynamically linked to OpenSSL');

const secLevel = require('internal/crypto/util').getOpenSSLSecLevel();
const assert = require('assert');

// Node.js 22 was released with OpenSSL 3.0 which had a default security
// level of 1. OpenSSL 3.2 bumped this to 2, but we need to fix this at
// 1 to minimize disruption to users of Node.js 22.x.
assert.strictEqual(secLevel, 1);
