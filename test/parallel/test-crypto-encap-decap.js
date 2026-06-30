'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');
const { hasOpenSSL } = require('../common/crypto');
const { promisify } = require('util');

const isBoringSSL = process.features.openssl_is_boringssl;

if (!hasOpenSSL(3) && !isBoringSSL) {
  assert.throws(() => crypto.encapsulate(), { code: 'ERR_CRYPTO_KEM_NOT_SUPPORTED' });
  return;
}

assert.throws(() => crypto.encapsulate(), { code: 'ERR_INVALID_ARG_TYPE',
                                            message: /The "key" argument must be of type/ });
assert.throws(() => crypto.decapsulate(), { code: 'ERR_INVALID_ARG_TYPE',
                                            message: /The "key" argument must be of type/ });

const keys = {
  'rsa': {
    supported: hasOpenSSL(3), // RSASVE was added in 3.0
    publicKey: fixtures.readKey('rsa_public_2048.pem', 'ascii'),
    privateKey: fixtures.readKey('rsa_private_2048.pem', 'ascii'),
    sharedSecretLength: 256,
    ciphertextLength: 256,
    raw: false,
  },
  'rsa-pss': {
    supported: false, // Only raw RSA is supported
    publicKey: fixtures.readKey('rsa_pss_public_2048.pem', 'ascii'),
    privateKey: fixtures.readKey('rsa_pss_private_2048.pem', 'ascii'),
  },
  'p-256': {
    supported: hasOpenSSL(3, 2), // DHKEM was added in 3.2
    publicKey: fixtures.readKey('ec_p256_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ec_p256_private.pem', 'ascii'),
    sharedSecretLength: 32,
    ciphertextLength: 65,
    raw: true,
  },
  'p-384': {
    supported: hasOpenSSL(3, 2), // DHKEM was added in 3.2
    publicKey: fixtures.readKey('ec_p384_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ec_p384_private.pem', 'ascii'),
    sharedSecretLength: 48,
    ciphertextLength: 97,
    raw: true,
  },
  'p-521': {
    supported: hasOpenSSL(3, 2), // DHKEM was added in 3.2
    publicKey: fixtures.readKey('ec_p521_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ec_p521_private.pem', 'ascii'),
    sharedSecretLength: 64,
    ciphertextLength: 133,
    raw: true,
  },
  'secp256k1': {
    supported: false, // only P-256, P-384, and P-521 are supported
    publicKey: fixtures.readKey('ec_secp256k1_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ec_secp256k1_private.pem', 'ascii'),
  },
  'x25519': {
    supported: hasOpenSSL(3, 2), // DHKEM was added in 3.2
    publicKey: fixtures.readKey('x25519_public.pem', 'ascii'),
    privateKey: fixtures.readKey('x25519_private.pem', 'ascii'),
    sharedSecretLength: 32,
    ciphertextLength: 32,
    raw: true,
  },
  'x448': {
    supported: hasOpenSSL(3, 2), // DHKEM was added in 3.2
    publicKey: fixtures.readKey('x448_public.pem', 'ascii'),
    privateKey: fixtures.readKey('x448_private.pem', 'ascii'),
    sharedSecretLength: 64,
    ciphertextLength: 56,
    raw: true,
  },
  'ml-kem-512': {
    supported: hasOpenSSL(3, 5),  // BoringSSL does not support ML-KEM-512
    publicKey: fixtures.readKey('ml_kem_512_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ml_kem_512_private_seed_only.pem', 'ascii'),
    sharedSecretLength: 32,
    ciphertextLength: 768,
    raw: true,
  },
  'ml-kem-768': {
    supported: hasOpenSSL(3, 5) || isBoringSSL,
    publicKey: fixtures.readKey('ml_kem_768_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ml_kem_768_private_seed_only.pem', 'ascii'),
    sharedSecretLength: 32,
    ciphertextLength: 1088,
    raw: true,
  },
  'ml-kem-1024': {
    supported: hasOpenSSL(3, 5) || isBoringSSL,
    publicKey: fixtures.readKey('ml_kem_1024_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ml_kem_1024_private_seed_only.pem', 'ascii'),
    sharedSecretLength: 32,
    ciphertextLength: 1568,
    raw: true,
  },
};

for (const [name, {
  supported, publicKey, privateKey, sharedSecretLength, ciphertextLength, raw,
}] of Object.entries(keys)) {
  if (!supported) {
    assert.throws(() => crypto.encapsulate(publicKey),
                  { code: /ERR_OSSL_EVP_DECODE_ERROR|ERR_OSSL_EVP_UNSUPPORTED_ALGORITHM|ERR_CRYPTO_OPERATION_FAILED/ });
    continue;
  }

  {
    assert.throws(() => crypto.decapsulate(privateKey, null),
                  {
                    code: 'ERR_INVALID_ARG_TYPE',
                    message: /instance of ArrayBuffer, Buffer, TypedArray, or DataView\. Received null/
                  });
  }

  // Derandomized (FIPS 203, 6.2) encapsulation: same `entropy` => same output,
  // and the result round-trips back to the same shared secret.
  if (name.startsWith('ml-')) {
    const entropy = Buffer.alloc(32, 0x42);
    const r1 = crypto.encapsulate(publicKey, { entropy });
    const r2 = crypto.encapsulate(publicKey, { entropy });
    assert(r1.ciphertext.equals(r2.ciphertext));
    assert(r1.sharedKey.equals(r2.sharedKey));
    assert.strictEqual(r1.ciphertext.byteLength, ciphertextLength);
    assert.strictEqual(r1.sharedKey.byteLength, sharedSecretLength);
    const sk = crypto.decapsulate(privateKey, r1.ciphertext);
    assert(sk.equals(r1.sharedKey));

    // A different `entropy` yields a different ciphertext.
    const r3 = crypto.encapsulate(publicKey, { entropy: Buffer.alloc(32, 0x24) });
    assert(!r3.ciphertext.equals(r1.ciphertext));

    // entropy must be exactly 32 bytes (FIPS 203, 6.2 message length) — both
    // bounds, and an explicit empty buffer is an invalid length, not the
    // randomized path.
    assert.throws(() => crypto.encapsulate(publicKey, { entropy: Buffer.alloc(31) }),
                  { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => crypto.encapsulate(publicKey, { entropy: Buffer.alloc(33) }),
                  { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => crypto.encapsulate(publicKey, { entropy: Buffer.alloc(0) }),
                  { code: 'ERR_OUT_OF_RANGE' });

    // An absent or undefined entropy selects the randomized path and must not
    // throw or abort (the 7th binding argument is undefined here).
    assert.strictEqual(
      crypto.encapsulate(publicKey, { entropy: undefined }).ciphertext.byteLength,
      ciphertextLength);
    assert.strictEqual(
      crypto.encapsulate(publicKey).ciphertext.byteLength, ciphertextLength);

    // Non-byte-source entropy is rejected by getArrayBufferOrView.
    assert.throws(() => crypto.encapsulate(publicKey, { entropy: null }),
                  {
                    code: 'ERR_INVALID_ARG_TYPE',
                    message: /instance of ArrayBuffer, Buffer, TypedArray, or DataView\. Received null/
                  });

    // options must be an object.
    assert.throws(() => crypto.encapsulate(publicKey, 'nope'),
                  { code: 'ERR_INVALID_ARG_TYPE' });
  }

  function formatKeyAs(key, params) {
    return { ...params, key: key.export(params) };
  }

  const keyObjects = {
    publicKey: crypto.createPublicKey(publicKey),
    privateKey: crypto.createPrivateKey(privateKey),
  };

  const keyPairs = [
    keyObjects,
    { publicKey, privateKey },
    {
      publicKey: formatKeyAs(keyObjects.publicKey, { format: 'der', type: 'spki' }),
      privateKey: formatKeyAs(keyObjects.privateKey, { format: 'der', type: 'pkcs8' })
    },
    {
      publicKey: formatKeyAs(keyObjects.publicKey, { format: 'jwk' }),
      privateKey: formatKeyAs(keyObjects.privateKey, { format: 'jwk' })
    },
  ];

  if (raw) {
    const { asymmetricKeyType } = keyObjects.privateKey;
    const { namedCurve } = keyObjects.privateKey.asymmetricKeyDetails;
    const privateFormat = asymmetricKeyType.startsWith('ml-') ? 'raw-seed' : 'raw-private';
    const rawPublic = {
      key: keyObjects.publicKey.export({ format: 'raw-public' }),
      format: 'raw-public',
      asymmetricKeyType,
      ...(namedCurve ? { namedCurve } : {}),
    };
    const rawPrivate = {
      key: keyObjects.privateKey.export({ format: privateFormat }),
      format: privateFormat,
      asymmetricKeyType,
      ...(namedCurve ? { namedCurve } : {}),
    };
    keyPairs.push({ publicKey: rawPublic, privateKey: rawPrivate });
  }

  for (const kp of keyPairs) {
    // sync
    {
      const { sharedKey, ciphertext } = crypto.encapsulate(kp.publicKey);
      assert(Buffer.isBuffer(sharedKey));
      assert.strictEqual(sharedKey.byteLength, sharedSecretLength);
      assert(Buffer.isBuffer(ciphertext));
      assert.strictEqual(ciphertext.byteLength, ciphertextLength);
      const sharedKey2 = crypto.decapsulate(kp.privateKey, ciphertext);
      assert(Buffer.isBuffer(sharedKey2));
      assert.strictEqual(sharedKey2.byteLength, sharedSecretLength);
      assert(sharedKey.equals(sharedKey2));
    }

    // async
    {
      crypto.encapsulate(kp.publicKey, common.mustSucceed(({ sharedKey, ciphertext }) => {
        assert(Buffer.isBuffer(sharedKey));
        assert.strictEqual(sharedKey.byteLength, sharedSecretLength);
        assert(Buffer.isBuffer(ciphertext));
        assert.strictEqual(ciphertext.byteLength, ciphertextLength);
        crypto.decapsulate(kp.privateKey, ciphertext, common.mustSucceed((sharedKey2) => {
          assert(Buffer.isBuffer(sharedKey2));
          assert.strictEqual(sharedKey2.byteLength, sharedSecretLength);
          assert(sharedKey.equals(sharedKey2));
        }));
      }));
    }

    // promisified
    (async () => {
      const { sharedKey, ciphertext } = await promisify(crypto.encapsulate)(kp.publicKey);
      assert(Buffer.isBuffer(sharedKey));
      assert.strictEqual(sharedKey.byteLength, sharedSecretLength);
      assert(Buffer.isBuffer(ciphertext));
      assert.strictEqual(ciphertext.byteLength, ciphertextLength);
      const sharedKey2 = await promisify(crypto.decapsulate)(kp.privateKey, ciphertext);
      assert(Buffer.isBuffer(sharedKey2));
      assert.strictEqual(sharedKey2.byteLength, sharedSecretLength);
      assert(sharedKey.equals(sharedKey2));
    })().then(common.mustCall());
  }

  let wrongPrivateKey;
  if (name.startsWith('x')) {
    wrongPrivateKey = name === 'x448' ? keys.x25519.privateKey : keys.x448.privateKey;
  } else if (name.startsWith('p-')) {
    wrongPrivateKey = name === 'p-256' ? keys['p-384'].privateKey : keys['p-256'].privateKey;
  } else if (name.startsWith('ml-')) {
    wrongPrivateKey = name === 'ml-kem-768' ? keys['ml-kem-1024'].privateKey : keys['ml-kem-768'].privateKey;
  } else {
    wrongPrivateKey = keys.x25519.privateKey;
  }

  // sync errors
  {
    const { ciphertext } = crypto.encapsulate(publicKey);
    assert.throws(() => crypto.decapsulate(wrongPrivateKey, ciphertext), {
      message: /Decapsulation failed/,
      code: 'ERR_CRYPTO_OPERATION_FAILED',
    });
  }

  // async errors
  {
    crypto.encapsulate(publicKey, common.mustSucceed(({ ciphertext }) => {
      crypto.decapsulate(wrongPrivateKey, ciphertext, common.mustCall((err) => {
        assert(err);
        assert.match(err.message, /Decapsulation failed/);
        assert.strictEqual(err.code, 'ERR_CRYPTO_OPERATION_FAILED');
      }));
    }));
  }
}
