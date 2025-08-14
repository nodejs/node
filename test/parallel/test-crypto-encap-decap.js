'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');
const { hasOpenSSL } = require('../common/crypto');
const { promisify } = require('util');

if (!hasOpenSSL(3)) {
  assert.throws(() => crypto.encapsulate(), { code: 'ERR_CRYPTO_KEM_NOT_SUPPORTED' });
  return;
}

assert.throws(() => crypto.encapsulate(), { code: 'ERR_CRYPTO_KEY_REQUIRED' });
assert.throws(() => crypto.decapsulate(), { code: 'ERR_CRYPTO_KEY_REQUIRED' });

const keys = {
  'rsa': {
    supported: hasOpenSSL(3), // RSASVE was added in 3.0
    publicKey: fixtures.readKey('rsa_public_2048.pem', 'ascii'),
    privateKey: fixtures.readKey('rsa_private_2048.pem', 'ascii'),
    sharedSecretLength: 256,
    ciphertextLength: 256,
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
  },
  'p-384': {
    supported: hasOpenSSL(3, 2), // DHKEM was added in 3.2
    publicKey: fixtures.readKey('ec_p384_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ec_p384_private.pem', 'ascii'),
    sharedSecretLength: 48,
    ciphertextLength: 97,
  },
  'p-521': {
    supported: hasOpenSSL(3, 2), // DHKEM was added in 3.2
    publicKey: fixtures.readKey('ec_p521_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ec_p521_private.pem', 'ascii'),
    sharedSecretLength: 64,
    ciphertextLength: 133,
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
  },
  'x448': {
    supported: hasOpenSSL(3, 2), // DHKEM was added in 3.2
    publicKey: fixtures.readKey('x448_public.pem', 'ascii'),
    privateKey: fixtures.readKey('x448_private.pem', 'ascii'),
    sharedSecretLength: 64,
    ciphertextLength: 56,
  },
  'ml-kem-512': {
    supported: hasOpenSSL(3, 5),
    publicKey: fixtures.readKey('ml_kem_512_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ml_kem_512_private.pem', 'ascii'),
    sharedSecretLength: 32,
    ciphertextLength: 768,
  },
  'ml-kem-768': {
    supported: hasOpenSSL(3, 5),
    publicKey: fixtures.readKey('ml_kem_768_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ml_kem_768_private.pem', 'ascii'),
    sharedSecretLength: 32,
    ciphertextLength: 1088,
  },
  'ml-kem-1024': {
    supported: hasOpenSSL(3, 5),
    publicKey: fixtures.readKey('ml_kem_1024_public.pem', 'ascii'),
    privateKey: fixtures.readKey('ml_kem_1024_private.pem', 'ascii'),
    sharedSecretLength: 32,
    ciphertextLength: 1568,
  },
};

for (const [name, { supported, publicKey, privateKey, sharedSecretLength, ciphertextLength }] of Object.entries(keys)) {
  if (!supported) {
    assert.throws(() => crypto.encapsulate(publicKey),
                  { code: /ERR_OSSL_EVP_DECODE_ERROR|ERR_CRYPTO_OPERATION_FAILED/ });
    continue;
  }

  {
    assert.throws(() => crypto.decapsulate(privateKey, null),
                  {
                    code: 'ERR_INVALID_ARG_TYPE',
                    message: /instance of ArrayBuffer, Buffer, TypedArray, or DataView\. Received null/
                  });
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
  ];

  // TODO(@panva): ML-KEM does not have a JWK format defined yet, add once standardized
  if (!keyObjects.privateKey.asymmetricKeyType.startsWith('ml')) {
    keyPairs.push({
      publicKey: formatKeyAs(keyObjects.publicKey, { format: 'jwk' }),
      privateKey: formatKeyAs(keyObjects.privateKey, { format: 'jwk' })
    });
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
    wrongPrivateKey = name === 'ml-kem-512' ? keys['ml-kem-768'].privateKey : keys['ml-kem-512'].privateKey;
  } else {
    wrongPrivateKey = keys.x25519.privateKey;
  }

  // sync errors
  {
    const { ciphertext } = crypto.encapsulate(publicKey);
    assert.throws(() => crypto.decapsulate(wrongPrivateKey, ciphertext), {
      message: /Failed to (initialize|perform) decapsulation/,
      code: 'ERR_CRYPTO_OPERATION_FAILED',
    });
  }

  // async errors
  {
    crypto.encapsulate(publicKey, common.mustSucceed(({ ciphertext }) => {
      crypto.decapsulate(wrongPrivateKey, ciphertext, common.mustCall((err) => {
        assert(err);
        assert.strictEqual(err.message, 'Deriving bits failed');
      }));
    }));
  }
}
