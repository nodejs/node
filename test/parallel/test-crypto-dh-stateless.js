'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { test, testDH, testDHError } = require('../common/crypto-dh');
const {
  hasOpenSSL,
  isBoringSSL: commonIsBoringSSL,
} = require('../common/crypto');
const isBoringSSL = commonIsBoringSSL;

// Error code for a key-type mismatch during (EC)DH. The underlying OpenSSL
// error code varies by version, and in OpenSSL 4.0 by platform: some builds
// report a generic internal error instead of a typed key-type mismatch.
// https://github.com/openssl/openssl/issues/30895
// TODO(panva): Tighten this check once/if fixed.
let keyTypeMismatchCode;
if (hasOpenSSL(4, 0)) {
  keyTypeMismatchCode =
    /^ERR_OSSL_EVP_(OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE|INTERNAL_ERROR)$/;
} else if (hasOpenSSL(3)) {
  keyTypeMismatchCode = 'ERR_OSSL_EVP_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE';
} else {
  keyTypeMismatchCode = 'ERR_OSSL_EVP_DIFFERENT_KEY_TYPES';
}

assert.throws(() => crypto.diffieHellman(), {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "options" argument must be of type object. Received undefined'
});

assert.throws(() => crypto.diffieHellman(null), {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "options" argument must be of type object. Received null'
});

assert.throws(() => crypto.diffieHellman([]), {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message:
    'The "options" argument must be of type object. ' +
    'Received an instance of Array',
});

assert.throws(() => crypto.diffieHellman(
  crypto.generateKeyPairSync('ec', { namedCurve: 'P-256' }), null), {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "callback" argument must be of type function. Received null'
});

{
  const kp = {
    privateKey: crypto.generateKeySync('aes', { length: 128 }),
    publicKey: crypto.generateKeyPairSync('x25519').publicKey,
  };

  assert.throws(() => {
    test(kp, kp);
  }, {
    code: 'ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE',
    message: 'Invalid key object type secret, expected private.'
  });
}

{
  const kp = {
    privateKey: crypto.generateKeyPairSync('x25519').publicKey,
    publicKey: crypto.generateKeyPairSync('x25519').privateKey,
  };

  assert.throws(() => {
    test(kp, kp);
  }, {
    code: 'ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE',
    message: 'Invalid key object type public, expected private.'
  });
}

{
  const { publicKey: pub } = crypto.generateKeyPairSync('x25519');

  assert.throws(() => {
    crypto.diffieHellman({
      privateKey: pub,
      publicKey: pub,
    });
  }, {
    code: 'ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE',
    message: 'Invalid key object type public, expected private.'
  });
}

{
  const kp = {
    privateKey: crypto.generateKeyPairSync('x25519').privateKey,
    publicKey: crypto.generateKeySync('aes', { length: 128 }),
  };

  assert.throws(() => {
    test(kp, kp);
  }, {
    code: 'ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE',
    message: 'Invalid key object type secret, expected private or public.'
  });
}

// Test that error messages include the correct property path
{
  const kp = crypto.generateKeyPairSync('x25519');
  const pub = kp.publicKey.export({ type: 'spki', format: 'pem' });
  const priv = kp.privateKey.export({ type: 'pkcs8', format: 'pem' });

  // Invalid privateKey format
  assert.throws(() => crypto.diffieHellman({
    privateKey: { key: Buffer.alloc(0), format: 'banana', type: 'pkcs8' },
    publicKey: pub,
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /options\.privateKey\.format/,
  });

  // Invalid privateKey type
  assert.throws(() => crypto.diffieHellman({
    privateKey: { key: Buffer.alloc(0), format: 'der', type: 'banana' },
    publicKey: pub,
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /options\.privateKey\.type/,
  });

  // Invalid publicKey format
  assert.throws(() => crypto.diffieHellman({
    publicKey: { key: Buffer.alloc(0), format: 'banana', type: 'spki' },
    privateKey: priv,
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /options\.publicKey\.format/,
  });

  // Invalid publicKey type
  assert.throws(() => crypto.diffieHellman({
    publicKey: { key: Buffer.alloc(0), format: 'der', type: 'banana' },
    privateKey: priv,
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /options\.publicKey\.type/,
  });
}

{
  const { privateKey, publicKey } = crypto.generateKeyPairSync('ec', {
    namedCurve: 'P-256',
  });

  assert.throws(() => crypto.diffieHellman({ privateKey }), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => crypto.diffieHellman({ publicKey }), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

// Test ECDH.

test(crypto.generateKeyPairSync('ec', { namedCurve: 'P-256' }),
     crypto.generateKeyPairSync('ec', { namedCurve: 'P-256' }));

{
  const options = {
    privateKey: crypto.generateKeyPairSync('ec', { namedCurve: 'P-256' }).privateKey,
    publicKey: crypto.generateKeyPairSync('ec', { namedCurve: 'P-384' }).publicKey,
  };
  testDHError(options, {
    name: 'Error',
    code: hasOpenSSL(3) ?
      'ERR_OSSL_MISMATCHING_DOMAIN_PARAMETERS' :
      'ERR_OSSL_EVP_DIFFERENT_PARAMETERS'
  });
}

if (isBoringSSL) {
  common.printSkipMessage('Skipping x448 diffieHellman test cases ' +
                          'unsupported by BoringSSL');
} else {
  test(crypto.generateKeyPairSync('x448'),
       crypto.generateKeyPairSync('x448'));

  {
    const options = {
      privateKey: crypto.generateKeyPairSync('x448').privateKey,
      publicKey: crypto.generateKeyPairSync('x25519').publicKey,
    };
    testDHError(options, { code: keyTypeMismatchCode });
  }
}

test(crypto.generateKeyPairSync('x25519'),
     crypto.generateKeyPairSync('x25519'));

// Test all key encoding formats
for (const { privateKey: alicePriv, publicKey: bobPub } of [
  crypto.generateKeyPairSync('ec', { namedCurve: 'P-256' }),
  crypto.generateKeyPairSync('x25519'),
]) {
  const expected = crypto.diffieHellman({
    privateKey: alicePriv,
    publicKey: bobPub,
  });

  const encodings = [
    // PEM string
    {
      privateKey: alicePriv.export({ type: 'pkcs8', format: 'pem' }),
      publicKey: bobPub.export({ type: 'spki', format: 'pem' }),
    },
    // PEM { key, format } object
    {
      privateKey: {
        key: alicePriv.export({ type: 'pkcs8', format: 'pem' }),
        format: 'pem',
      },
      publicKey: {
        key: bobPub.export({ type: 'spki', format: 'pem' }),
        format: 'pem',
      },
    },
    // DER PKCS#8 / SPKI
    {
      privateKey: {
        key: alicePriv.export({ type: 'pkcs8', format: 'der' }),
        format: 'der',
        type: 'pkcs8',
      },
      publicKey: {
        key: bobPub.export({ type: 'spki', format: 'der' }),
        format: 'der',
        type: 'spki',
      },
    },
    // JWK
    {
      privateKey: { key: alicePriv.export({ format: 'jwk' }), format: 'jwk' },
      publicKey: { key: bobPub.export({ format: 'jwk' }), format: 'jwk' },
    },
    // Raw key material
    {
      privateKey: {
        key: alicePriv.export({ format: 'raw-private' }),
        format: 'raw-private',
        asymmetricKeyType: alicePriv.asymmetricKeyType,
        ...alicePriv.asymmetricKeyDetails,
      },
      publicKey: {
        key: bobPub.export({ format: 'raw-public' }),
        format: 'raw-public',
        asymmetricKeyType: bobPub.asymmetricKeyType,
        ...bobPub.asymmetricKeyDetails,
      },
    },
  ];

  // EC-only encodings
  if (alicePriv.asymmetricKeyType === 'ec') {
    // DER SEC1 private key
    encodings.push({
      privateKey: {
        key: alicePriv.export({ type: 'sec1', format: 'der' }),
        format: 'der',
        type: 'sec1',
      },
      publicKey: bobPub,
    });
    // Raw with compressed public key
    encodings.push({
      privateKey: {
        key: alicePriv.export({ format: 'raw-private' }),
        format: 'raw-private',
        asymmetricKeyType: 'ec',
        ...alicePriv.asymmetricKeyDetails,
      },
      publicKey: {
        key: bobPub.export({ format: 'raw-public', type: 'compressed' }),
        format: 'raw-public',
        asymmetricKeyType: 'ec',
        ...bobPub.asymmetricKeyDetails,
      },
    });
  }

  for (const options of encodings) {
    testDH(options, expected);
  }
}

// Test C++ error conditions (both sync throws and async callback)
{
  const ec256 = crypto.generateKeyPairSync('ec', { namedCurve: 'P-256' });
  const ec384 = crypto.generateKeyPairSync('ec', { namedCurve: 'P-384' });
  const x448 = isBoringSSL ? null : crypto.generateKeyPairSync('x448');
  const x25519 = crypto.generateKeyPairSync('x25519');
  const ed25519 = crypto.generateKeyPairSync('ed25519');

  const zeroX25519PublicKey = crypto.createPublicKey('-----BEGIN PUBLIC KEY-----\n' +
    'MCowBQYDK2VuAyEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\n' +
    '-----END PUBLIC KEY-----');

  const encodings = [
    {
      privateKey: (key) => key.export({ type: 'pkcs8', format: 'pem' }),
      publicKey: (key) => key.export({ type: 'spki', format: 'pem' }),
    },
    {
      privateKey: (key) => key.export({ type: 'pkcs8', format: 'pem' }),
      publicKey: (key) => key,
    },
    {
      privateKey: (key) => key,
      publicKey: (key) => key.export({ type: 'spki', format: 'pem' }),
    },
    {
      privateKey: (key) => key,
      publicKey: (key) => key,
    },
  ];

  for (const { privateKey: privKey, publicKey: pubKey } of encodings) {
    // Mismatching EC curves
    testDHError({
      privateKey: privKey(ec256.privateKey),
      publicKey: pubKey(ec384.publicKey),
    }, { code: hasOpenSSL(3) ?
      'ERR_OSSL_MISMATCHING_DOMAIN_PARAMETERS' :
      'ERR_OSSL_EVP_DIFFERENT_PARAMETERS' });

    // Incompatible key types (ec + x25519)
    testDHError({
      privateKey: privKey(ec256.privateKey),
      publicKey: pubKey(x25519.publicKey),
    }, { code: keyTypeMismatchCode });

    // Unsupported key type (ed25519)
    testDHError({
      privateKey: privKey(ed25519.privateKey),
      publicKey: pubKey(ed25519.publicKey),
    }, { code: hasOpenSSL(4, 0) ?
      /^ERR_OSSL_EVP_(OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE|INTERNAL_ERROR)$/ :
      'ERR_OSSL_EVP_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE' });

    if (!isBoringSSL) {
      // Incompatible key types (x448 + x25519)
      testDHError({
        privateKey: privKey(x448.privateKey),
        publicKey: pubKey(x25519.publicKey),
      }, { code: keyTypeMismatchCode });
    }

    // Zero x25519 public key
    testDHError({
      privateKey: privKey(x25519.privateKey),
      publicKey: pubKey(zeroX25519PublicKey),
    }, isBoringSSL ? { code: 'ERR_OSSL_EVP_INVALID_PEER_KEY' } :
      hasOpenSSL(3) ?
        { code: 'ERR_OSSL_FAILED_DURING_DERIVATION' } :
        { message: /Deriving bits failed/ });
  }
}
