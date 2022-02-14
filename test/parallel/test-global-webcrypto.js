// Flags: --experimental-global-webcrypto
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

assert.strictEqual(globalThis.crypto, crypto.webcrypto);
assert.strictEqual(Crypto, crypto.webcrypto.constructor);
assert.strictEqual(SubtleCrypto, crypto.webcrypto.subtle.constructor);
