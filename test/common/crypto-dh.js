'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

// Runs diffieHellman for key pairs in both directions,
// verifies results match, and checks both sync and async paths.
function test({ publicKey: alicePublicKey, privateKey: alicePrivateKey },
              { publicKey: bobPublicKey, privateKey: bobPrivateKey },
              expectedValue) {
  const buf1 = crypto.diffieHellman({
    privateKey: alicePrivateKey,
    publicKey: bobPublicKey,
  });
  const buf2 = crypto.diffieHellman({
    privateKey: bobPrivateKey,
    publicKey: alicePublicKey,
  });
  const buf3 = crypto.diffieHellman({
    privateKey: bobPrivateKey,
    publicKey: alicePrivateKey,
  });
  assert.deepStrictEqual(buf1, buf2);
  assert.deepStrictEqual(buf1, buf3);

  if (expectedValue !== undefined)
    assert.deepStrictEqual(buf1, expectedValue);

  // Verify async produces the same results
  crypto.diffieHellman({
    privateKey: alicePrivateKey,
    publicKey: bobPublicKey,
  }, common.mustSucceed((asyncBuf) => {
    assert.deepStrictEqual(asyncBuf, buf1);
  }));
  crypto.diffieHellman({
    privateKey: bobPrivateKey,
    publicKey: alicePublicKey,
  }, common.mustSucceed((asyncBuf) => {
    assert.deepStrictEqual(asyncBuf, buf1);
  }));
}

// Verifies diffieHellman succeeds sync and async with expected result.
function testDH(options, expected) {
  const syncResult = crypto.diffieHellman(options);
  if (expected !== undefined) {
    assert.deepStrictEqual(syncResult, expected);
  }
  crypto.diffieHellman(options, common.mustSucceed((asyncResult) => {
    assert.deepStrictEqual(asyncResult, syncResult);
  }));
}

// Verifies diffieHellman fails with expected error, both sync and async.
function testDHError(options, expected) {
  assert.throws(() => crypto.diffieHellman(options), expected);
  crypto.diffieHellman(options, common.mustCall((err) => {
    assert.ok(err);
    for (const [key, value] of Object.entries(expected)) {
      if (value instanceof RegExp) {
        assert.match(err[key], value);
      } else {
        assert.strictEqual(err[key], value);
      }
    }
  }));
}

module.exports = { test, testDH, testDHError };
