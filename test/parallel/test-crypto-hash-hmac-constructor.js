'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

// The Hash and Hmac constructors are not intended for public use. Instances
// must be created with crypto.createHash() and crypto.createHmac().
const illegalConstructor = {
  code: 'ERR_ILLEGAL_CONSTRUCTOR',
  name: 'TypeError',
};

assert.throws(() => crypto.Hash('sha256'), illegalConstructor);
assert.throws(() => new crypto.Hash('sha256'), illegalConstructor);
assert.throws(() => crypto.Hmac('sha256', 'key'), illegalConstructor);
assert.throws(() => new crypto.Hmac('sha256', 'key'), illegalConstructor);

// The classes remain exported so that instanceof checks keep working.
{
  const hash = crypto.createHash('sha256');
  assert(hash instanceof crypto.Hash);
  assert(hash.copy() instanceof crypto.Hash);
  assert(crypto.createHmac('sha256', 'key') instanceof crypto.Hmac);
}
