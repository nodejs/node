'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const tls = require('tls');

// A session that fails to deserialize must not leave an error on the OpenSSL
// error queue for the next unrelated crypto operation to pick up.

const badKey = '-----BEGIN PRIVATE KEY-----\nAAAA\n-----END PRIVATE KEY-----';

function keyErrorCode() {
  try {
    crypto.createPrivateKey(badKey);
  } catch (err) {
    return err.code;
  }
  assert.fail('createPrivateKey() should have thrown');
}

const expected = keyErrorCode();
new tls.TLSSocket(null, { session: Buffer.from('invalid session') });
assert.strictEqual(keyErrorCode(), expected);
