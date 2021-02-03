'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createPrivateKey,
  createPublicKey,
} = require('crypto');

const {
  sign,
  verify,
} = require('crypto/promises');

const fixtures = require('../common/fixtures');

async function testRsaPss() {
  // Test cryptoPromises.(sign/verify) w/ RSA-PSS.
  {
    // This key pair does not restrict the message digest algorithm or salt
    // length.
    const publicPem = fixtures.readKey('rsa_pss_public_2048.pem');
    const privatePem = fixtures.readKey('rsa_pss_private_2048.pem');

    const publicKey = createPublicKey(publicPem);
    const privateKey = createPrivateKey(privatePem);

    // Any algorithm should work.
    for (const algo of ['sha1', 'sha256']) {
      // Any salt length should work.
      for (const saltLength of [undefined, 8, 10, 12, 16, 18, 20]) {
        const signature = await sign(
          algo, Buffer.from('foo'), privateKey, { saltLength });

        for (const key of [publicKey, privateKey]) {
          const okay = await verify(
            algo, Buffer.from('foo'), key, signature, { saltLength });

          assert.ok(okay);
        }
      }
    }
  }

  {
    // This key pair enforces sha256 as the message digest and the MGF1
    // message digest and a salt length of at least 16 bytes.
    const publicPem =
      fixtures.readKey('rsa_pss_public_2048_sha256_sha256_16.pem');
    const privatePem =
      fixtures.readKey('rsa_pss_private_2048_sha256_sha256_16.pem');

    const publicKey = createPublicKey(publicPem);
    const privateKey = createPrivateKey(privatePem);

    // Signing with anything other than sha256 should fail.
    await assert.rejects(() => sign('sha1', Buffer.from('foo'), privateKey), /digest not allowed/);

    // Signing with salt lengths less than 16 bytes should fail.
    for (const saltLength of [8, 10, 12]) {
      await assert.rejects(() => sign('sha256', Buffer.from('foo'), privateKey, { saltLength }), /pss saltlen too small/);
    }

    // Signing with sha256 and appropriate salt lengths should work.
    for (const saltLength of [undefined, 16, 18, 20]) {
      const signature = await sign(
        'sha256', Buffer.from('foo'), privateKey, { saltLength });

      for (const key of [publicKey, privateKey]) {
        const okay = await verify(
          'sha256', Buffer.from('foo'), key, signature, { saltLength });

        assert.ok(okay);
      }
    }
  }

  {
    // This key enforces sha512 as the message digest and sha256 as the MGF1
    // message digest.
    const publicPem =
      fixtures.readKey('rsa_pss_public_2048_sha512_sha256_20.pem');
    const privatePem =
      fixtures.readKey('rsa_pss_private_2048_sha512_sha256_20.pem');

    const publicKey = createPublicKey(publicPem);
    const privateKey = createPrivateKey(privatePem);

    // Node.js usually uses the same hash function for the message and for MGF1.
    // However, when a different MGF1 message digest algorithm has been
    // specified as part of the key, it should automatically switch to that.
    // This behavior is required by sections 3.1 and 3.3 of RFC4055.

    // sha256 matches the MGF1 hash function and should be used internally,
    // but it should not be permitted as the main message digest algorithm.
    for (const algo of ['sha1', 'sha256']) {
      await assert.rejects(() => sign(algo, Buffer.from('foo'), privateKey), /digest not allowed/);
    }

    // sha512 should produce a valid signature.
    const signature = await sign('sha512', Buffer.from('foo'), privateKey);

    for (const pkey of [publicKey, privateKey]) {
      const okay = await verify('sha512', Buffer.from('foo'), pkey, signature);

      assert.ok(okay);
    }
  }
}

testRsaPss().then(common.mustCall());
