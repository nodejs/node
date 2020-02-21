'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  constants,
  createSign,
  createVerify,
  generateKeyPair,
  generateKeyPairSync,
  publicEncrypt,
  privateDecrypt,
  sign,
  verify
} = require('crypto');
const { promisify } = require('util');

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
  // Test sync key generation with key objects.
  const { publicKey, privateKey } = generateKeyPairSync('rsa', {
    modulusLength: 512
  });

  assert.strictEqual(typeof publicKey, 'object');
  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(publicKey.asymmetricKeyType, 'rsa');

  assert.strictEqual(typeof privateKey, 'object');
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(privateKey.asymmetricKeyType, 'rsa');
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
  }, common.mustCall((err, publicKeyDER, privateKey) => {
    assert.ifError(err);

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
    publicExponent: 0x1001,
    modulusLength: 512,
    publicKeyEncoding,
    privateKeyEncoding: {
      type: 'pkcs1',
      format: 'pem',
      cipher: 'aes-256-cbc',
      passphrase: 'secret'
    }
  }, common.mustCall((err, publicKeyDER, privateKey) => {
    assert.ifError(err);

    assert(Buffer.isBuffer(publicKeyDER));
    assertApproximateSize(publicKeyDER, 74);

    assert.strictEqual(typeof privateKey, 'string');
    assert(pkcs1EncExp('AES-256-CBC').test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    const publicKey = { key: publicKeyDER, ...publicKeyEncoding };
    assert.throws(() => testSignVerify(publicKey, privateKey), {
      name: 'TypeError',
      code: 'ERR_MISSING_PASSPHRASE',
      message: 'Passphrase required for encrypted key'
    });

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
  }, common.mustCall((err, publicKeyDER, privateKeyDER) => {
    assert.ifError(err);

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
  }, common.mustCall((err, publicKeyDER, privateKeyDER) => {
    assert.ifError(err);

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
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

    assert.strictEqual(publicKey.type, 'public');
    assert.strictEqual(publicKey.asymmetricKeyType, 'rsa-pss');

    assert.strictEqual(privateKey.type, 'private');
    assert.strictEqual(privateKey.asymmetricKeyType, 'rsa-pss');

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
    modulusLength: 512,
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
  }, common.mustCall((err, publicKey, privateKeyDER) => {
    assert.ifError(err);

    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    // The private key is DER-encoded.
    assert(Buffer.isBuffer(privateKeyDER));

    assertApproximateSize(publicKey, 440);
    assertApproximateSize(privateKeyDER, 336);

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
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

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
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

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
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(sec1EncExp('AES-128-CBC').test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => testSignVerify(publicKey, privateKey), {
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
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(sec1EncExp('AES-128-CBC').test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => testSignVerify(publicKey, privateKey), {
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
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(pkcs8EncExp.test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => testSignVerify(publicKey, privateKey), {
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
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

    assert.strictEqual(typeof publicKey, 'string');
    assert(spkiExp.test(publicKey));
    assert.strictEqual(typeof privateKey, 'string');
    assert(pkcs8EncExp.test(privateKey));

    // Since the private key is encrypted, signing shouldn't work anymore.
    assert.throws(() => testSignVerify(publicKey, privateKey), {
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
    code: 'ERR_INVALID_OPT_VALUE',
    message: 'The value "otherEncoding" is invalid for ' +
    'option "paramEncoding"'
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
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

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
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${enc}" is invalid for option "publicKeyEncoding"`
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${type}" is invalid for option ` +
               '"publicKeyEncoding.type"'
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${format}" is invalid for option ` +
               '"publicKeyEncoding.format"'
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${enc}" is invalid for option "privateKeyEncoding"`
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${type}" is invalid for option ` +
               '"privateKeyEncoding.type"'
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${format}" is invalid for option ` +
               '"privateKeyEncoding.format"'
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${cipher}" is invalid for option ` +
               '"privateKeyEncoding.cipher"'
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${passphrase}" is invalid for option ` +
               '"privateKeyEncoding.passphrase"'
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
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${modulusLength}" is invalid for option ` +
               '"modulusLength"'
    });
  }

  // Test invalid exponents.
  for (const publicExponent of ['a', true, {}, [], 3.5, -1]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength: 4096,
      publicExponent
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${publicExponent}" is invalid for option ` +
               '"publicExponent"'
    });
  }
}

// Test DSA parameters.
{
  // Test invalid modulus lengths.
  for (const modulusLength of [undefined, null, 'a', true, {}, [], 4096.1]) {
    assert.throws(() => generateKeyPair('dsa', {
      modulusLength
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${modulusLength}" is invalid for option ` +
               '"modulusLength"'
    });
  }

  // Test invalid divisor lengths.
  for (const divisorLength of ['a', true, {}, [], 4096.1]) {
    assert.throws(() => generateKeyPair('dsa', {
      modulusLength: 2048,
      divisorLength
    }), {
      name: 'TypeError',
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${divisorLength}" is invalid for option ` +
               '"divisorLength"'
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
    message: 'Invalid ECDH curve name'
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${namedCurve}" is invalid for option ` +
               '"namedCurve"'
    });
  }

  // It should recognize both NIST and standard curve names.
  generateKeyPair('ec', {
    namedCurve: 'P-256',
    publicKeyEncoding: { type: 'spki', format: 'pem' },
    privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);
  }));

  generateKeyPair('ec', {
    namedCurve: 'secp256k1',
    publicKeyEncoding: { type: 'spki', format: 'pem' },
    privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);
  }));
}

// Test EdDSA key generation.
{
  if (!/^1\.1\.0/.test(process.versions.openssl)) {
    ['ed25519', 'ed448', 'x25519', 'x448'].forEach((keyType) => {
      generateKeyPair(keyType, common.mustCall((err, publicKey, privateKey) => {
        assert.ifError(err);

        assert.strictEqual(publicKey.type, 'public');
        assert.strictEqual(publicKey.asymmetricKeyType, keyType);

        assert.strictEqual(privateKey.type, 'private');
        assert.strictEqual(privateKey.asymmetricKeyType, keyType);
      }));
    });
  }
}

// Test classic Diffie-Hellman key generation.
{
  generateKeyPair('dh', {
    primeLength: 1024
  }, common.mustCall((err, publicKey, privateKey) => {
    assert.ifError(err);

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
    ['prime', 'primeLength']
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${type}" is invalid for option ` +
               '"publicKeyEncoding.type"'
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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${hashValue}" is invalid for option "hash"`
    });
  }

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
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${type}" is invalid for option ` +
               '"privateKeyEncoding.type"'
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
        });
      },
      {
        name: 'TypeError',
        code: 'ERR_INVALID_OPT_VALUE',
        message: `The value "${mgf1Hash}" is invalid for option "mgf1Hash"`
      }
    );
  }
}
