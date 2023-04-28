// Flags: --no-experimental-global-webcrypto
'use strict';

require('../common');
const assert = require('assert');

assert.strictEqual(typeof crypto, 'undefined');
assert.strictEqual(typeof Crypto, 'undefined');
assert.strictEqual(typeof CryptoKey, 'undefined');
assert.strictEqual(typeof SubtleCrypto, 'undefined');
