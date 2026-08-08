'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

// DEP0206 reached end-of-life: calling Hmac.digest() on a finalized instance
// now throws ERR_CRYPTO_HASH_FINALIZED instead of returning an empty buffer.
// See: https://github.com/nodejs/node/pull/65112

// Repeated digest() on the same instance throws.
{
  const h = crypto.createHmac('sha1', 'key').update('data');
  assert.strictEqual(h.digest('hex').length, 40);
  assert.throws(() => h.digest('hex'), { code: 'ERR_CRYPTO_HASH_FINALIZED' });
}

// digest() with no data throws on a second call.
{
  const h = crypto.createHmac('sha1', 'key');
  assert.strictEqual(h.digest('buffer').length, 20);
  assert.throws(() => h.digest('buffer'), { code: 'ERR_CRYPTO_HASH_FINALIZED' });
}

// digest() after the Hmac has been used as a stream throws.
{
  const h = crypto.createHmac('sha1', 'key');
  h.end('data');
  assert.strictEqual(h.read().length, 20);
  assert.throws(() => h.digest('buffer'), { code: 'ERR_CRYPTO_HASH_FINALIZED' });
}
