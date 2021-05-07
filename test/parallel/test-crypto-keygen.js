'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.hasOpenSSL3)
  common.skip('temporarily skipping for OpenSSL 3.0-alpha15');

const assert = require('assert');
const {
  constants,
  createPrivateKey,
  createSign,
  createVerify,
  generateKeyPair,
  generateKeyPairSync,
  getCurves,
  publicEncrypt,
  privateDecrypt,
  sign,
  verify
} = require('crypto');
const { inspect, promisify } = require('util');

// Asserts that the size of the given key (in chars or bytes) is within 10% of
// the expected size.
function assertApproximateSize(key, expectedSize) {
  const u = typeof key === 'string' ? 'chars' : 'bytes';
  const min = Math.floor(0.9 * expectedSize);
  const max = Math.ceil(1.1 * expectedSize);
  assert(key.length >= min,
         `Key (${key.length} ${u}) is shorter than expected (${min} ${u})`);
  assert(key.length <= max,
         `Key (${key.length} ${u}) is longer than expected (${max} ${u})`);
}

// Tests that a key pair can be used for encryption / decryption.
function testEncryptDecrypt(publicKey, privateKey) {
  const message = 'Hello Node.js world!';
  const plaintext = Buffer.from(message, 'utf8');
  for (const key of [publicKey, privateKey]) {
    const ciphertext = publicEncrypt(key, plaintext);
    const received = privateDecrypt(privateKey, ciphertext);
    assert.strictEqual(received.toString('utf8'), message);
  }
}

// Tests that a key pair can be used for signing / verification.
function testSignVerify(publicKey, privateKey) {
  const message = Buffer.from('Hello Node.js world!');

  function oldSign(algo, data, key) {
    return createSign(algo).update(data).sign(key);
  }

  function oldVerify(algo, data, key, signature) {
    return createVerify(algo).update(data).verify(key, signature);
  }

  for (const signFn of [sign, oldSign]) {
    const signature = signFn('SHA256', message, privateKey);
    for (const verifyFn of [verify, oldVerify]) {
      for (const key of [publicKey, privateKey]) {
        const okay = verifyFn('SHA256', message, key, signature);
        assert(okay);
      }
    }
  }
}

// Constructs a regular expression for a PEM-encoded key with the given label.
function getRegExpForPEM(label, cipher) {
  const head = `\\-\\-\\-\\-\\-BEGIN ${label}\\-\\-\\-\\-\\-`;
  const rfc1421Header = cipher == null ? '' :
    `\nProc-Type: 4,ENCRYPTED\nDEK-Info: ${cipher},[^\n]+\n`;
  const body = '([a-zA-Z0-9\\+/=]{64}\n)*[a-zA-Z0-9\\+/=]{1,64}';
  const end = `\\-\\-\\-\\-\\-END ${label}\\-\\-\\-\\-\\-`;
  return new RegExp(`^${head}${rfc1421Header}\n${body}\n${end}\n$`);
}

const pkcs1PubExp = getRegExpForPEM('RSA PUBLIC KEY');
const pkcs1PrivExp = getRegExpForPEM('RSA PRIVATE KEY');
const pkcs1EncExp = (cipher) => getRegExpForPEM('RSA PRIVATE KEY', cipher);
const spkiExp = getRegExpForPEM('PUBLIC KEY');
const pkcs8Exp = getRegExpForPEM('PRIVATE KEY');
const pkcs8EncExp = getRegExpForPEM('ENCRYPTED PRIVATE KEY');
const sec1Exp = getRegExpForPEM('EC PRIVATE KEY');
const sec1EncExp = (cipher) => getRegExpForPEM('EC PRIVATE KEY', cipher);

{
  // To make the test faster, we will only test sync key generation once and
  // with a relatively small key.
  const ret = generateKeyPairSync('rsa', {
    publicExponent: 3,
    modulusLength: 512,
    publicKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'pkcs8',
      format: 'pem'
    }
  });

  assert.strictEqual(Object.keys(ret).length, 2);
  const { publicKey, privateKey } = ret;

  assert.strictEqual(typeof publicKey, 'string');
  assert(pkcs1PubExp.test(publicKey));
  assertApproximateSize(publicKey, 162);
  assert.strictEqual(typeof privateKey, 'string');
  assert(pkcs8Exp.test(privateKey));
  assertApproximateSize(privateKey, 512);

  testEncryptDecrypt(publicKey, privateKey);
  testSignVerify(publicKey, privateKey);
}

{
  // Test sync key generation with key objects with a non-standard
  // publicExpononent
  const { publicKey, privateKey } = generateKeyPairSync('rsa', {
    publicExponent: 3,
    modulusLength: 512
  });

  assert.strictEqual(typeof publicKey, 'object');
  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(publicKey.asymmetricKeyType, 'rsa');
  assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
    modulusLength: 512,
    publicExponent: 3n
  });

  assert.strictEqual(typeof privateKey, 'object');
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(privateKey.asymmetricKeyType, 'rsa');
  assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
    modulusLength: 512,
    publicExponent: 3n
  });
}

{
  // Test sync key generation with key objects.
  const { publicKey, privateKey } = generateKeyPairSync('rsa', {
    modulusLength: 512
  });

  assert.strictEqual(typeof publicKey, 'object');
  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(publicKey.asymmetricKeyType, 'rsa');
  assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
    modulusLength: 512,
    publicExponent: 65537n
  });

  assert.strictEqual(typeof privateKey, 'object');
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(privateKey.asymmetricKeyType, 'rsa');
  assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
    modulusLength: 512,
    publicExponent: 65537n
  });
}

{
  const publicKeyEncoding = {
    type: 'pkcs1',
    format: 'der'
  };

  // Test async RSA key generation.
  generateKeyPair('rsa', {
    publicExponent: 0x10001,
    modulusLength: 512,
    publicKeyEncoding,
    privateKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    }
  }, common.mustSucceed((publicKeyDER, privateKey) => {
    assert(Buffer.isBuffer(publicKeyDER));
    assertApproximateSize(publicKeyDER, 74);

    assert.strictEqual(typeof privateKey, 'string');
    assert(pkcs1PrivExp.test(privateKey));
    assertApproximateSize(privateKey, 512);

    const publicKey = { key: publicKeyDER, ...publicKeyEncoding };
    testEncryptDecrypt(publicKey, privateKey);
    testSignVerify(publicKey, privateKey);
  }));

  // Now do the same with an encrypted private key.
  generateKeyPair('rsa', {
    publicExponent: 0x10001,
    modulusLength: 512,
    publicKeyEncoding,
    privateKeyEncoding: {
      type: 'pkcs1',
      format: 'pem',
      cipher: 'aes-256-cbc',
      passphrase: 'secret'
    }
  }, common.mustSucceed((publicKeyDER, privateKey) => {
    assert(Buffer.isBuffer(publicKeyDER));
    assertApproximateSize(publicKeyDER, 74);

    assert.strictEqual(typeof privateKey, 'string');
    assert(pkcs1EncExp('AES-256-CBC').test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    const publicKey = { key: publicKeyDER, ...publicKeyEncoding };
    const expectedError = common.hasOpenSSL3 ? {
      name: 'Error',
      message: 'error:07880109:common libcrypto routines::interrupted or ' +
               'cancelled'
    } : {
      name: 'TypeError',
      code: 'ERR_MISSING_PASSPHRASE',
      message: 'Passphrase required for encrypted key'
    };
    assert.throws(() => testSignVerify(publicKey, privateKey), expectedError);

    const key = { key: privateKey, passphrase: 'secret' };
    testEncryptDecrypt(publicKey, key);
    testSignVerify(publicKey, key);
  }));

  // Now do the same with an encrypted private key, but encoded as DER.
  generateKeyPair('rsa', {
    publicExponent: 0x10001,
    modulusLength: 512,
    publicKeyEncoding,
    privateKeyEncoding: {
      type: 'pkcs8',
      format: 'der',
      cipher: 'aes-256-cbc',
      passphrase: 'secret'
    }
  }, common.mustSucceed((publicKeyDER, privateKeyDER) => {
    assert(Buffer.isBuffer(publicKeyDER));
    assertApproximateSize(publicKeyDER, 74);

    assert(Buffer.isBuffer(privateKeyDER));

    // Since the private key is encrypted, signing shouldn't work anymore.
    const publicKey = { key: publicKeyDER, ...publicKeyEncoding };
    assert.throws(() => {
      testSignVerify(publicKey, {
        key: privateKeyDER,
        format: 'der',
        type: 'pkcs8'
      });
    }, {
      name: 'TypeError',
      code: 'ERR_MISSING_PASSPHRASE',
      message: 'Passphrase required for encrypted key'
    });

    // Signing should work with the correct password.

    const privateKey = {
      key: privateKeyDER,
      format: 'der',
      type: 'pkcs8',
      passphrase: 'secret'
    };
    testEncryptDecrypt(publicKey, privateKey);
    testSignVerify(publicKey, privateKey);
  }));

  // Now do the same with an encrypted private key, but encoded as DER.
  generateKeyPair('rsa', {
    publicExponent: 0x10001,
    modulusLength: 512,
    publicKeyEncoding,
    privateKeyEncoding: {
      type: 'pkcs8',
      format: 'der'
    }
  }, common.mustSucceed((publicKeyDER, privateKeyDER) => {
    assert(Buffer.isBuffer(publicKeyDER));
    assertApproximateSize(publicKeyDER, 74);

    assert(Buffer.isBuffer(privateKeyDER));

    const publicKey = { key: publicKeyDER, ...publicKeyEncoding };
    const privateKey = {
      key: privateKeyDER,
      format: 'der',
      type: 'pkcs8',
      passphrase: 'secret'
    };
    testEncryptDecrypt(publicKey, privateKey);
    testSignVerify(publicKey, privateKey);
  }));
}

{
  // Test RSA-PSS.
  generateKeyPair('rsa-pss', {
    modulusLength: 512,
    saltLength: 16,
    hash: 'sha256',
    mgf1Hash: 'sha256'
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'rsa-pss');
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      modulusLength: 512,
      publicExponent: 65537n
    });

    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'rsa-pss');
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
      modulusLength: 512,
      publicExponent: 65537n
    });

    // Unlike RSA, RSA-PSS does not allow encryption.
    assert.throws(() => {
      testEncryptDecrypt(publicKey, privateKey);
    }, /operation not supported for this keytype/);

    // RSA-PSS also does not permit signing with PKCS1 padding.
    assert.throws(() => {
      testSignVerify({
        key: publicKey,
        padding: constants.RSA_PKCS1_PADDING
      }, {
        key: privateKey,
        padding: constants.RSA_PKCS1_PADDING
      });
    }, /illegal or unsupported padding mode/);

    // The padding should correctly default to RSA_PKCS1_PSS_PADDING now.
    testSignVerify(publicKey, privateKey);
  }));
}

{
  const privateKeyEncoding = {
    type: 'pkcs8',
    format: 'der'
  };

  // Test async DSA key generation.
  generateKeyPair('dsa', {
    modulusLength: common.hasOpenSSL3 ? 2048 : 512,
    divisorLength: 256,
    publicKeyEncoding: {
      type: 'spki',
      format: 'pem'
    },
    privateKeyEncoding: {
      cipher: 'aes-128-cbc',
      passphrase: 'secret',
      ...privateKeyEncoding
    }
  }, common.mustSucceed((publicKey, privateKeyDER) => {
    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    // The private key is DER-encoded.
    assert(Buffer.isBuffer(privateKeyDER));

    assertApproximateSize(publicKey, common.hasOpenSSL3 ? 1194 : 440);
    assertApproximateSize(privateKeyDER, common.hasOpenSSL3 ? 721 : 336);

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => {
      return testSignVerify(publicKey, {
        key: privateKeyDER,
        ...privateKeyEncoding
      });
    }, {
      name: 'TypeError',
      code: 'ERR_MISSING_PASSPHRASE',
      message: 'Passphrase required for encrypted key'
    });

    // Signing should work with the correct password.
    testSignVerify(publicKey, {
      key: privateKeyDER,
      ...privateKeyEncoding,
      passphrase: 'secret'
    });
  }));
}
{
  // Test async DSA key object generation.
  generateKeyPair('dsa', {
    modulusLength: common.hasOpenSSL3 ? 2048 : 512,
    divisorLength: 256
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'dsa');
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      modulusLength: common.hasOpenSSL3 ? 2048 : 512,
      divisorLength: 256
    });

    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'dsa');
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
      modulusLength: common.hasOpenSSL3 ? 2048 : 512,
      divisorLength: 256
    });
  }));
}

{
  // Test async elliptic curve key generation, e.g. for ECDSA, with a SEC1
  // private key.
  generateKeyPair('ec', {
    namedCurve: 'prime256v1',
    paramEncoding: 'named',
    publicKeyEncoding: {
      type: 'spki',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'sec1',
      format: 'pem'
    }
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(sec1Exp.test(privateKey));

    testSignVerify(publicKey, privateKey);
  }));

  // Test async elliptic curve key generation, e.g. for ECDSA, with a SEC1
  // private key with paramEncoding explicit.
  generateKeyPair('ec', {
    namedCurve: 'prime256v1',
    paramEncoding: 'explicit',
    publicKeyEncoding: {
      type: 'spki',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'sec1',
      format: 'pem'
    }
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(sec1Exp.test(privateKey));

    testSignVerify(publicKey, privateKey);
  }));

  // Do the same with an encrypted private key.
  generateKeyPair('ec', {
    namedCurve: 'prime256v1',
    paramEncoding: 'named',
    publicKeyEncoding: {
      type: 'spki',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'sec1',
      format: 'pem',
      cipher: 'aes-128-cbc',
      passphrase: 'secret'
    }
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(sec1EncExp('AES-128-CBC').test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => testSignVerify(publicKey, privateKey),
                  common.hasOpenSSL3 ? {
                    message: 'error:07880109:common libcrypto ' +
                             'routines::interrupted or cancelled'
                  } : {
                    name: 'TypeError',
                    code: 'ERR_MISSING_PASSPHRASE',
                    message: 'Passphrase required for encrypted key'
                  });

    testSignVerify(publicKey, { key: privateKey, passphrase: 'secret' });
  }));

  // Do the same with an encrypted private key with paramEncoding explicit.
  generateKeyPair('ec', {
    namedCurve: 'prime256v1',
    paramEncoding: 'explicit',
    publicKeyEncoding: {
      type: 'spki',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'sec1',
      format: 'pem',
      cipher: 'aes-128-cbc',
      passphrase: 'secret'
    }
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(sec1EncExp('AES-128-CBC').test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => testSignVerify(publicKey, privateKey),
                  common.hasOpenSSL3 ? {
                    message: 'error:07880109:common libcrypto ' +
                             'routines::interrupted or cancelled'
                  } : {
                    name: 'TypeError',
                    code: 'ERR_MISSING_PASSPHRASE',
                    message: 'Passphrase required for encrypted key'
                  });

    testSignVerify(publicKey, { key: privateKey, passphrase: 'secret' });
  }));
}

{
  // Test async elliptic curve key generation, e.g. for ECDSA, with an encrypted
  // private key.
  generateKeyPair('ec', {
    namedCurve: 'P-256',
    paramEncoding: 'named',
    publicKeyEncoding: {
      type: 'spki',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'pkcs8',
      format: 'pem',
      cipher: 'aes-128-cbc',
      passphrase: 'top secret'
    }
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(pkcs8EncExp.test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => testSignVerify(publicKey, privateKey),
                  common.hasOpenSSL3 ? {
                    message: 'error:07880109:common libcrypto ' +
                    'routines::interrupted or cancelled'
                  } : {
                    name: 'TypeError',
                    code: 'ERR_MISSING_PASSPHRASE',
                    message: 'Passphrase required for encrypted key'
                  });

    testSignVerify(publicKey, {
      key: privateKey,
      passphrase: 'top secret'
    });
  }));

  // Test async elliptic curve key generation, e.g. for ECDSA, with an encrypted
  // private key with paramEncoding explicit.
  generateKeyPair('ec', {
    namedCurve: 'P-256',
    paramEncoding: 'explicit',
    publicKeyEncoding: {
      type: 'spki',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'pkcs8',
      format: 'pem',
      cipher: 'aes-128-cbc',
      passphrase: 'top secret'
    }
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(pkcs8EncExp.test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => testSignVerify(publicKey, privateKey),
                  common.hasOpenSSL3 ? {
                    message: 'error:07880109:common libcrypto ' +
                    'routines::interrupted or cancelled'
                  } : {
                    name: 'TypeError',
                    code: 'ERR_MISSING_PASSPHRASE',
                    message: 'Passphrase required for encrypted key'
                  });

    testSignVerify(publicKey, {
      key: privateKey,
      passphrase: 'top secret'
    });
  }));
}

// Test invalid parameter encoding.
{
  assert.throws(() => generateKeyPairSync('ec', {
    namedCurve: 'P-256',
    paramEncoding: 'otherEncoding',
    publicKeyEncoding: {
      type: 'spki',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'pkcs8',
      format: 'pem',
      cipher: 'aes-128-cbc',
      passphrase: 'top secret'
    }
  }), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The property 'options.paramEncoding' is invalid. " +
      "Received 'otherEncoding'"
  });
}

{
  // Test the util.promisified API with async RSA key generation.
  promisify(generateKeyPair)('rsa', {
    publicExponent: 0x10001,
    modulusLength: 512,
    publicKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    }
  }).then(common.mustCall((keys) => {
    const { publicKey, privateKey } = keys;
    assert.strictEqual(typeof publicKey, 'string');
    assert(pkcs1PubExp.test(publicKey));
    assertApproximateSize(publicKey, 180);

    assert.strictEqual(typeof privateKey, 'string');
    assert(pkcs1PrivExp.test(privateKey));
    assertApproximateSize(privateKey, 512);

    testEncryptDecrypt(publicKey, privateKey);
    testSignVerify(publicKey, privateKey);
  }));
}

{
  // Test invalid key types.
  for (const type of [undefined, null, 0]) {
    assert.throws(() => generateKeyPairSync(type, {}), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "type" argument must be of type string.' +
               common.invalidArgTypeHelper(type)
    });
  }

  assert.throws(() => generateKeyPairSync('rsa2', {}), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The argument 'type' must be a supported key type. Received 'rsa2'"
  });
}

{
  // Test keygen without options object.
  assert.throws(() => generateKeyPair('rsa', common.mustNotCall()), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options" argument must be of type object. ' +
      'Received undefined'
  });

  // Even if no options are required, it should be impossible to pass anything
  // but an object (or undefined).
  assert.throws(() => generateKeyPair('ed448', 0, common.mustNotCall()), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options" argument must be of type object. ' +
      'Received type number (0)'
  });
}

{
  // If no publicKeyEncoding is specified, a key object should be returned.
  generateKeyPair('rsa', {
    modulusLength: 1024,
    privateKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    }
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(typeof publicKey, 'object');
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'rsa');

    // The private key should still be a string.
    assert.strictEqual(typeof privateKey, 'string');

    testEncryptDecrypt(publicKey, privateKey);
    testSignVerify(publicKey, privateKey);
  }));

  // If no privateKeyEncoding is specified, a key object should be returned.
  generateKeyPair('rsa', {
    modulusLength: 1024,
    publicKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    }
  }, common.mustSucceed((publicKey, privateKey) => {
    // The public key should still be a string.
    assert.strictEqual(typeof publicKey, 'string');

    assert.strictEqual(typeof privateKey, 'object');
    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'rsa');

    testEncryptDecrypt(publicKey, privateKey);
    testSignVerify(publicKey, privateKey);
  }));
}

{
  // Invalid publicKeyEncoding.
  for (const enc of [0, 'a', true]) {
    assert.throws(() => generateKeyPairSync('rsa', {
      modulusLength: 4096,
      publicKeyEncoding: enc,
      privateKeyEncoding: {
        type: 'pkcs1',
        format: 'pem'
      }
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.publicKeyEncoding' is invalid. " +
        `Received ${inspect(enc)}`
    });
  }

  // Missing publicKeyEncoding.type.
  for (const type of [undefined, null, 0, true, {}]) {
    assert.throws(() => generateKeyPairSync('rsa', {
      modulusLength: 4096,
      publicKeyEncoding: {
        type,
        format: 'pem'
      },
      privateKeyEncoding: {
        type: 'pkcs1',
        format: 'pem'
      }
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.publicKeyEncoding.type' is invalid. " +
        `Received ${inspect(type)}`
    });
  }

  // Missing / invalid publicKeyEncoding.format.
  for (const format of [undefined, null, 0, false, 'a', {}]) {
    assert.throws(() => generateKeyPairSync('rsa', {
      modulusLength: 4096,
      publicKeyEncoding: {
        type: 'pkcs1',
        format
      },
      privateKeyEncoding: {
        type: 'pkcs1',
        format: 'pem'
      }
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.publicKeyEncoding.format' is invalid. " +
        `Received ${inspect(format)}`
    });
  }

  // Invalid privateKeyEncoding.
  for (const enc of [0, 'a', true]) {
    assert.throws(() => generateKeyPairSync('rsa', {
      modulusLength: 4096,
      publicKeyEncoding: {
        type: 'pkcs1',
        format: 'pem'
      },
      privateKeyEncoding: enc
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.privateKeyEncoding' is invalid. " +
        `Received ${inspect(enc)}`
    });
  }

  // Missing / invalid privateKeyEncoding.type.
  for (const type of [undefined, null, 0, true, {}]) {
    assert.throws(() => generateKeyPairSync('rsa', {
      modulusLength: 4096,
      publicKeyEncoding: {
        type: 'pkcs1',
        format: 'pem'
      },
      privateKeyEncoding: {
        type,
        format: 'pem'
      }
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.privateKeyEncoding.type' is invalid. " +
        `Received ${inspect(type)}`
    });
  }

  // Missing / invalid privateKeyEncoding.format.
  for (const format of [undefined, null, 0, false, 'a', {}]) {
    assert.throws(() => generateKeyPairSync('rsa', {
      modulusLength: 4096,
      publicKeyEncoding: {
        type: 'pkcs1',
        format: 'pem'
      },
      privateKeyEncoding: {
        type: 'pkcs1',
        format
      }
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.privateKeyEncoding.format' is invalid. " +
        `Received ${inspect(format)}`
    });
  }

  // Cipher of invalid type.
  for (const cipher of [0, true, {}]) {
    assert.throws(() => generateKeyPairSync('rsa', {
      modulusLength: 4096,
      publicKeyEncoding: {
        type: 'pkcs1',
        format: 'pem'
      },
      privateKeyEncoding: {
        type: 'pkcs1',
        format: 'pem',
        cipher
      }
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.privateKeyEncoding.cipher' is invalid. " +
        `Received ${inspect(cipher)}`
    });
  }

  // Invalid cipher.
  assert.throws(() => generateKeyPairSync('rsa', {
    modulusLength: 4096,
    publicKeyEncoding: {
      type: 'pkcs1',
      format: 'pem'
    },
    privateKeyEncoding: {
      type: 'pkcs8',
      format: 'pem',
      cipher: 'foo',
      passphrase: 'secret'
    }
  }), {
    name: 'Error',
    code: 'ERR_CRYPTO_UNKNOWN_CIPHER',
    message: 'Unknown cipher'
  });

  // Cipher, but no valid passphrase.
  for (const passphrase of [undefined, null, 5, false, true]) {
    assert.throws(() => generateKeyPairSync('rsa', {
      modulusLength: 4096,
      publicKeyEncoding: {
        type: 'pkcs1',
        format: 'pem'
      },
      privateKeyEncoding: {
        type: 'pkcs8',
        format: 'pem',
        cipher: 'aes-128-cbc',
        passphrase
      }
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.privateKeyEncoding.passphrase' " +
        `is invalid. Received ${inspect(passphrase)}`
    });
  }

  // Test invalid callbacks.
  for (const cb of [undefined, null, 0, {}]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength: 512,
      publicKeyEncoding: { type: 'pkcs1', format: 'pem' },
      privateKeyEncoding: { type: 'pkcs1', format: 'pem' }
    }, cb), {
      name: 'TypeError',
      code: 'ERR_INVALID_CALLBACK'
    });
  }
}

// Test RSA parameters.
{
  // Test invalid modulus lengths.
  for (const modulusLength of [undefined, null, 'a', true, {}, [], 512.1, -1]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength
    }, common.mustNotCall()), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.modulusLength' is invalid. " +
        `Received ${inspect(modulusLength)}`
    });
  }

  // Test invalid exponents.
  for (const publicExponent of ['a', true, {}, [], 3.5, -1]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength: 4096,
      publicExponent
    }, common.mustNotCall()), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.publicExponent' is invalid. " +
        `Received ${inspect(publicExponent)}`
    });
  }
}

// Test DSA parameters.
{
  // Test invalid modulus lengths.
  for (const modulusLength of [undefined, null, 'a', true, {}, [], 4096.1]) {
    assert.throws(() => generateKeyPair('dsa', {
      modulusLength
    }, common.mustNotCall()), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.modulusLength' is invalid. " +
        `Received ${inspect(modulusLength)}`
    });
  }

  // Test invalid divisor lengths.
  for (const divisorLength of ['a', true, {}, [], 4096.1, 2147483648, -1]) {
    assert.throws(() => generateKeyPair('dsa', {
      modulusLength: 2048,
      divisorLength
    }, common.mustNotCall()), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.divisorLength' is invalid. " +
        `Received ${inspect(divisorLength)}`
    });
  }
}

// Test EC parameters.
{
  // Test invalid curves.
  assert.throws(() => {
    generateKeyPairSync('ec', {
      namedCurve: 'abcdef',
      publicKeyEncoding: { type: 'spki', format: 'pem' },
      privateKeyEncoding: { type: 'sec1', format: 'pem' }
    });
  }, {
    name: 'TypeError',
    message: 'Invalid EC curve name'
  });

  // Test error type when curve is not a string
  for (const namedCurve of [true, {}, [], 123]) {
    assert.throws(() => {
      generateKeyPairSync('ec', {
        namedCurve,
        publicKeyEncoding: { type: 'spki', format: 'pem' },
        privateKeyEncoding: { type: 'sec1', format: 'pem' }
      });
    }, {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.namedCurve' is invalid. " +
        `Received ${inspect(namedCurve)}`
    });
  }

  // It should recognize both NIST and standard curve names.
  generateKeyPair('ec', {
    namedCurve: 'P-256',
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      namedCurve: 'prime256v1'
    });
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
      namedCurve: 'prime256v1'
    });
  }));

  generateKeyPair('ec', {
    namedCurve: 'secp256k1',
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      namedCurve: 'secp256k1'
    });
    assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {
      namedCurve: 'secp256k1'
    });
  }));
}

// Test EdDSA key generation.
{
  if (!/^1\.1\.0/.test(process.versions.openssl)) {
    ['ed25519', 'ed448', 'x25519', 'x448'].forEach((keyType) => {
      generateKeyPair(keyType, common.mustSucceed((publicKey, privateKey) => {
        assert.strictEqual(publicKey.type, 'public');
        assert.strictEqual(publicKey.asymmetricKeyType, keyType);
        assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {});

        assert.strictEqual(privateKey.type, 'private');
        assert.strictEqual(privateKey.asymmetricKeyType, keyType);
        assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {});
      }));
    });
  }
}

// Test classic Diffie-Hellman key generation.
{
  generateKeyPair('dh', {
    primeLength: 1024
  }, common.mustSucceed((publicKey, privateKey) => {
    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'dh');

    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'dh');
  }));

  assert.throws(() => {
    generateKeyPair('dh', common.mustNotCall());
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "options" argument must be of type object. Received undefined'
  });

  assert.throws(() => {
    generateKeyPair('dh', {}, common.mustNotCall());
  }, {
    name: 'TypeError',
    code: 'ERR_MISSING_OPTION',
    message: 'At least one of the group, prime, or primeLength options is ' +
             'required'
  });

  assert.throws(() => {
    generateKeyPair('dh', {
      group: 'modp0'
    }, common.mustNotCall());
  }, {
    name: 'Error',
    code: 'ERR_CRYPTO_UNKNOWN_DH_GROUP',
    message: 'Unknown DH group'
  });

  assert.throws(() => {
    generateKeyPair('dh', {
      primeLength: 2147483648
    }, common.mustNotCall());
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The property 'options.primeLength' is invalid. " +
             'Received 2147483648',
  });

  assert.throws(() => {
    generateKeyPair('dh', {
      primeLength: -1
    }, common.mustNotCall());
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The property 'options.primeLength' is invalid. " +
             'Received -1',
  });

  assert.throws(() => {
    generateKeyPair('dh', {
      primeLength: 2,
      generator: 2147483648,
    }, common.mustNotCall());
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The property 'options.generator' is invalid. " +
             'Received 2147483648',
  });

  assert.throws(() => {
    generateKeyPair('dh', {
      primeLength: 2,
      generator: -1,
    }, common.mustNotCall());
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The property 'options.generator' is invalid. " +
             'Received -1',
  });

  // Test incompatible options.
  const allOpts = {
    group: 'modp5',
    prime: Buffer.alloc(0),
    primeLength: 1024,
    generator: 2
  };
  const incompatible = [
    ['group', 'prime'],
    ['group', 'primeLength'],
    ['group', 'generator'],
    ['prime', 'primeLength'],
  ];
  for (const [opt1, opt2] of incompatible) {
    assert.throws(() => {
      generateKeyPairSync('dh', {
        [opt1]: allOpts[opt1],
        [opt2]: allOpts[opt2]
      });
    }, {
      name: 'TypeError',
      code: 'ERR_INCOMPATIBLE_OPTION_PAIR',
      message: `Option "${opt1}" cannot be used in combination with option ` +
               `"${opt2}"`
    });
  }
}

// Test invalid key encoding types.
{
  // Invalid public key type.
  for (const type of ['foo', 'pkcs8', 'sec1']) {
    assert.throws(() => {
      generateKeyPairSync('rsa', {
        modulusLength: 4096,
        publicKeyEncoding: { type, format: 'pem' },
        privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
      });
    }, {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.publicKeyEncoding.type' is invalid. " +
        `Received ${inspect(type)}`
    });
  }

  // Invalid hash value.
  for (const hashValue of [123, true, {}, []]) {
    assert.throws(() => {
      generateKeyPairSync('rsa-pss', {
        modulusLength: 4096,
        hash: hashValue
      });
    }, {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.hash' is invalid. " +
        `Received ${inspect(hashValue)}`
    });
  }

  // too long salt length
  assert.throws(() => {
    generateKeyPair('rsa-pss', {
      modulusLength: 512,
      saltLength: 2147483648,
      hash: 'sha256',
      mgf1Hash: 'sha256'
    }, common.mustNotCall());
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The property 'options.saltLength' is invalid. " +
             'Received 2147483648'
  });

  assert.throws(() => {
    generateKeyPair('rsa-pss', {
      modulusLength: 512,
      saltLength: -1,
      hash: 'sha256',
      mgf1Hash: 'sha256'
    }, common.mustNotCall());
  }, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "The property 'options.saltLength' is invalid. " +
             'Received -1'
  });

  // Invalid private key type.
  for (const type of ['foo', 'spki']) {
    assert.throws(() => {
      generateKeyPairSync('rsa', {
        modulusLength: 4096,
        publicKeyEncoding: { type: 'spki', format: 'pem' },
        privateKeyEncoding: { type, format: 'pem' }
      });
    }, {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_VALUE',
      message: "The property 'options.privateKeyEncoding.type' is invalid. " +
        `Received ${inspect(type)}`
    });
  }

  // Key encoding doesn't match key type.
  for (const type of ['dsa', 'ec']) {
    assert.throws(() => {
      generateKeyPairSync(type, {
        modulusLength: 4096,
        namedCurve: 'P-256',
        publicKeyEncoding: { type: 'pkcs1', format: 'pem' },
        privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
      });
    }, {
      name: 'Error',
      code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS',
      message: 'The selected key encoding pkcs1 can only be used for RSA keys.'
    });

    assert.throws(() => {
      generateKeyPairSync(type, {
        modulusLength: 4096,
        namedCurve: 'P-256',
        publicKeyEncoding: { type: 'spki', format: 'pem' },
        privateKeyEncoding: { type: 'pkcs1', format: 'pem' }
      });
    }, {
      name: 'Error',
      code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS',
      message: 'The selected key encoding pkcs1 can only be used for RSA keys.'
    });
  }

  for (const type of ['rsa', 'dsa']) {
    assert.throws(() => {
      generateKeyPairSync(type, {
        modulusLength: 4096,
        publicKeyEncoding: { type: 'spki', format: 'pem' },
        privateKeyEncoding: { type: 'sec1', format: 'pem' }
      });
    }, {
      name: 'Error',
      code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS',
      message: 'The selected key encoding sec1 can only be used for EC keys.'
    });
  }

  // Attempting to encrypt a DER-encoded, non-PKCS#8 key.
  for (const type of ['pkcs1', 'sec1']) {
    assert.throws(() => {
      generateKeyPairSync(type === 'pkcs1' ? 'rsa' : 'ec', {
        modulusLength: 4096,
        namedCurve: 'P-256',
        publicKeyEncoding: { type: 'spki', format: 'pem' },
        privateKeyEncoding: {
          type,
          format: 'der',
          cipher: 'aes-128-cbc',
          passphrase: 'hello'
        }
      });
    }, {
      name: 'Error',
      code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS',
      message: `The selected key encoding ${type} does not support encryption.`
    });
  }
}

{
  // Test RSA-PSS.
  assert.throws(
    () => {
      generateKeyPair('rsa-pss', {
        modulusLength: 512,
        saltLength: 16,
        hash: 'sha256',
        mgf1Hash: undefined
      });
    },
    {
      name: 'TypeError',
      code: 'ERR_INVALID_CALLBACK',
      message: 'Callback must be a function. Received undefined'
    }
  );

  for (const mgf1Hash of [null, 0, false, {}, []]) {
    assert.throws(
      () => {
        generateKeyPair('rsa-pss', {
          modulusLength: 512,
          saltLength: 16,
          hash: 'sha256',
          mgf1Hash
        }, common.mustNotCall());
      },
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_VALUE',
        message: "The property 'options.mgf1Hash' is invalid. " +
          `Received ${inspect(mgf1Hash)}`

      }
    );
  }
}

if (!common.hasOpenSSL3) {
  // Passing an empty passphrase string should not cause OpenSSL's default
  // passphrase prompt in the terminal.
  // See https://github.com/nodejs/node/issues/35898.

  for (const type of ['pkcs1', 'pkcs8']) {
    generateKeyPair('rsa', {
      modulusLength: 1024,
      privateKeyEncoding: {
        type,
        format: 'pem',
        cipher: 'aes-256-cbc',
        passphrase: ''
      }
    }, common.mustSucceed((publicKey, privateKey) => {
      assert.strictEqual(publicKey.type, 'public');

      for (const passphrase of ['', Buffer.alloc(0)]) {
        const privateKeyObject = createPrivateKey({
          passphrase,
          key: privateKey
        });
        assert.strictEqual(privateKeyObject.asymmetricKeyType, 'rsa');
      }

      // Encrypting with an empty passphrase is not the same as not encrypting
      // the key, and not specifying a passphrase should fail when decoding it.
      assert.throws(() => {
        return testSignVerify(publicKey, privateKey);
      }, {
        name: 'TypeError',
        code: 'ERR_MISSING_PASSPHRASE',
        message: 'Passphrase required for encrypted key'
      });
    }));
  }
}

{
  // Proprietary Web Cryptography API ECDH/ECDSA namedCurve parameters
  // should not be recognized in this API.
  // See https://github.com/nodejs/node/issues/37055
  const curves = ['NODE-ED25519', 'NODE-ED448', 'NODE-X25519', 'NODE-X448'];
  for (const namedCurve of curves) {
    assert.throws(
      () => generateKeyPair('ec', { namedCurve }, common.mustNotCall()),
      {
        name: 'TypeError',
        message: 'Invalid EC curve name'
      }
    );
  }
}

{
  // This test creates EC key pairs on curves without associated OIDs.
  // Specifying a key encoding should not crash.

  if (process.versions.openssl >= '1.1.1i') {
    for (const namedCurve of ['Oakley-EC2N-3', 'Oakley-EC2N-4']) {
      if (!getCurves().includes(namedCurve))
        continue;

      const expectedErrorCode =
        common.hasOpenSSL3 ? 'ERR_OSSL_MISSING_OID' : 'ERR_OSSL_EC_MISSING_OID';
      const params = {
        namedCurve,
        publicKeyEncoding: {
          format: 'der',
          type: 'spki'
        }
      };

      assert.throws(() => {
        generateKeyPairSync('ec', params);
      }, {
        code: expectedErrorCode
      });

      generateKeyPair('ec', params, common.mustCall((err) => {
        assert.strictEqual(err.code, expectedErrorCode);
      }));
    }
  }
}
