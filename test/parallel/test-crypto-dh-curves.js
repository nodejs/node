'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

// Second OAKLEY group, see
// https://github.com/nodejs/node-v0.x-archive/issues/2338 and
// https://xml2rfc.tools.ietf.org/public/rfc/html/rfc2412.html#anchor49
const p = 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74' +
          '020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437' +
          '4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED' +
          'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381FFFFFFFFFFFFFFFF';
crypto.createDiffieHellman(p, 'hex');

// Confirm DH_check() results are exposed for optional examination.
const bad_dh = crypto.createDiffieHellman('02', 'hex');
assert.notStrictEqual(bad_dh.verifyError, 0);

const availableCurves = new Set(crypto.getCurves());
const availableHashes = new Set(crypto.getHashes());

// Oakley curves do not clean up ERR stack, it was causing unexpected failure
// when accessing other OpenSSL APIs afterwards.
if (availableCurves.has('Oakley-EC2N-3')) {
  crypto.createECDH('Oakley-EC2N-3');
  crypto.createHash('sha256');
}

// Test ECDH
if (availableCurves.has('prime256v1') && availableCurves.has('secp256k1')) {
  const ecdh1 = crypto.createECDH('prime256v1');
  const ecdh2 = crypto.createECDH('prime256v1');
  const key1 = ecdh1.generateKeys();
  const key2 = ecdh2.generateKeys('hex');
  const secret1 = ecdh1.computeSecret(key2, 'hex', 'base64');
  const secret2 = ecdh2.computeSecret(key1, 'latin1', 'buffer');

  assert.strictEqual(secret1, secret2.toString('base64'));

  // Point formats
  assert.strictEqual(ecdh1.getPublicKey('buffer', 'uncompressed')[0], 4);
  let firstByte = ecdh1.getPublicKey('buffer', 'compressed')[0];
  assert(firstByte === 2 || firstByte === 3);
  firstByte = ecdh1.getPublicKey('buffer', 'hybrid')[0];
  assert(firstByte === 6 || firstByte === 7);
  // Format value should be string

  assert.throws(
    () => ecdh1.getPublicKey('buffer', 10),
    {
      code: 'ERR_CRYPTO_ECDH_INVALID_FORMAT',
      name: 'TypeError',
      message: 'Invalid ECDH format: 10'
    });

  // ECDH should check that point is on curve
  const ecdh3 = crypto.createECDH('secp256k1');
  const key3 = ecdh3.generateKeys();

  assert.throws(
    () => ecdh2.computeSecret(key3, 'latin1', 'buffer'),
    {
      code: 'ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY',
      name: 'Error',
      message: 'Public key is not valid for specified curve'
    });

  // ECDH should allow .setPrivateKey()/.setPublicKey()
  const ecdh4 = crypto.createECDH('prime256v1');

  ecdh4.setPrivateKey(ecdh1.getPrivateKey());
  ecdh4.setPublicKey(ecdh1.getPublicKey());

  assert.throws(() => {
    ecdh4.setPublicKey(ecdh3.getPublicKey());
  }, { message: 'Failed to convert Buffer to EC_POINT' });

  // Verify that we can use ECDH without having to use newly generated keys.
  const ecdh5 = crypto.createECDH('secp256k1');

  // Verify errors are thrown when retrieving keys from an uninitialized object.
  assert.throws(() => {
    ecdh5.getPublicKey();
  }, /^Error: Failed to get ECDH public key$/);

  assert.throws(() => {
    ecdh5.getPrivateKey();
  }, /^Error: Failed to get ECDH private key$/);

  // A valid private key for the secp256k1 curve.
  const cafebabeKey = 'cafebabe'.repeat(8);
  // Associated compressed and uncompressed public keys (points).
  const cafebabePubPtComp =
  '03672a31bfc59d3f04548ec9b7daeeba2f61814e8ccc40448045007f5479f693a3';
  const cafebabePubPtUnComp =
  '04672a31bfc59d3f04548ec9b7daeeba2f61814e8ccc40448045007f5479f693a3' +
  '2e02c7f93d13dc2732b760ca377a5897b9dd41a1c1b29dc0442fdce6d0a04d1d';
  ecdh5.setPrivateKey(cafebabeKey, 'hex');
  assert.strictEqual(ecdh5.getPrivateKey('hex'), cafebabeKey);
  // Show that the public point (key) is generated while setting the
  // private key.
  assert.strictEqual(ecdh5.getPublicKey('hex'), cafebabePubPtUnComp);

  // Compressed and uncompressed public points/keys for other party's
  // private key.
  // 0xDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF
  const peerPubPtComp =
  '02c6b754b20826eb925e052ee2c25285b162b51fdca732bcf67e39d647fb6830ae';
  const peerPubPtUnComp =
  '04c6b754b20826eb925e052ee2c25285b162b51fdca732bcf67e39d647fb6830ae' +
  'b651944a574a362082a77e3f2b5d9223eb54d7f2f76846522bf75f3bedb8178e';

  const sharedSecret =
  '1da220b5329bbe8bfd19ceef5a5898593f411a6f12ea40f2a8eead9a5cf59970';

  assert.strictEqual(ecdh5.computeSecret(peerPubPtComp, 'hex', 'hex'),
                     sharedSecret);
  assert.strictEqual(ecdh5.computeSecret(peerPubPtUnComp, 'hex', 'hex'),
                     sharedSecret);

  // Verify that we still have the same key pair as before the computation.
  assert.strictEqual(ecdh5.getPrivateKey('hex'), cafebabeKey);
  assert.strictEqual(ecdh5.getPublicKey('hex'), cafebabePubPtUnComp);

  // Verify setting and getting compressed and non-compressed serializations.
  ecdh5.setPublicKey(cafebabePubPtComp, 'hex');
  assert.strictEqual(ecdh5.getPublicKey('hex'), cafebabePubPtUnComp);
  assert.strictEqual(
    ecdh5.getPublicKey('hex', 'compressed'),
    cafebabePubPtComp
  );
  ecdh5.setPublicKey(cafebabePubPtUnComp, 'hex');
  assert.strictEqual(ecdh5.getPublicKey('hex'), cafebabePubPtUnComp);
  assert.strictEqual(
    ecdh5.getPublicKey('hex', 'compressed'),
    cafebabePubPtComp
  );

  // Show why allowing the public key to be set on this type
  // does not make sense.
  ecdh5.setPublicKey(peerPubPtComp, 'hex');
  assert.strictEqual(ecdh5.getPublicKey('hex'), peerPubPtUnComp);
  assert.throws(() => {
    // Error because the public key does not match the private key anymore.
    ecdh5.computeSecret(peerPubPtComp, 'hex', 'hex');
  }, /Invalid key pair/);

  // Set to a valid key to show that later attempts to set an invalid key are
  // rejected.
  ecdh5.setPrivateKey(cafebabeKey, 'hex');

  // Some invalid private keys for the secp256k1 curve.
  const errMessage = /Private key is not valid for specified curve/;
  ['0000000000000000000000000000000000000000000000000000000000000000',
   'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141',
   'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF',
  ].forEach((element) => {
    assert.throws(() => {
      ecdh5.setPrivateKey(element, 'hex');
    }, errMessage);
    // Verify object state did not change.
    assert.strictEqual(ecdh5.getPrivateKey('hex'), cafebabeKey);
  });
}

// Use of invalid keys was not cleaning up ERR stack, and was causing
// unexpected failure in subsequent signing operations.
if (availableCurves.has('prime256v1') && availableHashes.has('sha256')) {
  const curve = crypto.createECDH('prime256v1');
  const invalidKey = Buffer.alloc(65);
  invalidKey.fill('\0');
  curve.generateKeys();
  assert.throws(
    () => curve.computeSecret(invalidKey),
    {
      code: 'ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY',
      name: 'Error',
      message: 'Public key is not valid for specified curve'
    });
  // Check that signing operations are not impacted by the above error.
  const ecPrivateKey =
    '-----BEGIN EC PRIVATE KEY-----\n' +
    'MHcCAQEEIF+jnWY1D5kbVYDNvxxo/Y+ku2uJPDwS0r/VuPZQrjjVoAoGCCqGSM49\n' +
    'AwEHoUQDQgAEurOxfSxmqIRYzJVagdZfMMSjRNNhB8i3mXyIMq704m2m52FdfKZ2\n' +
    'pQhByd5eyj3lgZ7m7jbchtdgyOF8Io/1ng==\n' +
    '-----END EC PRIVATE KEY-----';
  crypto.createSign('SHA256').sign(ecPrivateKey);
}
