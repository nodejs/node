'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createPrivateKey,
  createPublicKey,
  generateKeyPairSync,
  getCurves,
  getHashes,
  randomBytes,
  sign,
  verify
}= require('crypto');


// Generate an SM2 key pair.
const {
  publicKey: sm2PublicKey,
  privateKey: sm2PrivateKey
} = generateKeyPairSync('sm2');

// While this key pair also uses the SM2 curve, it produces ECDSA signatures.
const {
  publicKey: ecdsaPublicKey,
  privateKey: ecdsaPrivateKey
} = generateKeyPairSync('ec', {
  namedCurve: 'SM2'
});


{
  // Even though the key structures and curves are exactly the same, their key
  // type should be different.

  assert.strictEqual(sm2PublicKey.asymmetricKeyType, 'sm2');
  assert.strictEqual(sm2PrivateKey.asymmetricKeyType, 'sm2');

  assert.strictEqual(ecdsaPublicKey.asymmetricKeyType, 'ec');
  assert.strictEqual(ecdsaPrivateKey.asymmetricKeyType, 'ec');
}

{
  // Test the basic behavior of SM2 signatures.

  const sizes = [0, 10, 100, 1000];

  for (const dataSize of [0, 10, 100, 1000]) {
    const data = randomBytes(dataSize);

    for (const idSize of [0, 10, 100, 1000]) {
      const sm2Identifier = randomBytes(idSize);
      const otherSm2Identifier = randomBytes(idSize === 0 ? 1 : idSize);

      // Generate valid signatures.
      const validSignatures = [null, 'SM3'].map((algo) => {
        return sign(algo, data, {
          key: sm2PrivateKey,
          sm2Identifier
        });
      });

      // Test the generated signatures.
      for (const signature of validSignatures) {
        for (const algo of [null, 'SM3']) {
          // Ensure that signatures are validated correctly.
          const signatureIsValid = verify(algo, data, {
            key: sm2PublicKey,
            sm2Identifier
          }, signature);
          assert.strictEqual(signatureIsValid, true);

          // Providing a different identifier should cause verification to fail.
          const isValidWithWrongId = verify(algo, data, {
            key: sm2PublicKey,
            sm2Identifier: otherSm2Identifier
          }, signature);
          assert.strictEqual(isValidWithWrongId, false);
        }
      }
    }
  }
}

{
  // Test that no hash functions other than SM3 are allowed.

  const data = randomBytes(100);
  const options = {
    key: sm2PrivateKey,
    sm2Identifier: randomBytes(10)
  };
  const sigBuf = Buffer.alloc(10);

  for (const hash of getHashes()) {
    if (!hash.startsWith('sm3') && !hash.endsWith('SM3')) {
      const args = [hash, data, options];
      for (const fn of [() => sign(...args), () => verify(...args, sigBuf)]) {
        assert.throws(fn, {
          code: 'ERR_CRYPTO_INVALID_DIGEST',
          name: 'TypeError'
        });
      }
    }
  }
}

{
  // Test without identifier.

  const possibleOptions = [
    sm2PrivateKey,
    { key: sm2PrivateKey },
    { key: sm2PrivateKey, sm2Identifier: null }
  ];
  const sigBuf = Buffer.alloc(10);

  for (const options of possibleOptions) {
    const args = [null, Buffer.alloc(0), options];
    for (const fn of [() => sign(...args), () => verify(...args, sigBuf)]) {
      assert.throws(fn, {
        code: 'ERR_MISSING_OPTION',
        message: 'sm2Identifier is required',
        name: 'TypeError'
      });
    }
  }
}

{
  // Test invalid identifiers.

  const key = sm2PrivateKey;
  const sigBuf = Buffer.alloc(10);

  for (const sm2Identifier of [true, false, 0, 1, () => {}, {}, []]) {
    const args = [null, Buffer.alloc(0), { key, sm2Identifier }];
    for (const fn of [() => sign(...args), () => verify(...args, sigBuf)]) {
      assert.throws(fn, {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      });
    }
  }
}

{
  // The type of existing KeyObjects cannot be changed.

  assert.throws(() => {
    createPublicKey({ key: sm2PrivateKey, asymmetricKeyType: 'ec' });
  }, {
    code: 'ERR_CRYPTO_INCOMPATIBLE_KEY',
    name: 'Error'
  });
}

{
  // Only EC keys with the SM2 curve can be imported as SM2 keys.

  const otherKeyArgs = [
    ['dh', { group: 'modp5' }],
    ['dsa', { modulusLength: 1024 }],
    ['ec', { namedCurve: 'P-256' }],
    ['ed25519'],
    ['ed448'],
    ['rsa', { modulusLength: 512, divisorLength: 256 }],
    ['x25519'],
    ['x448']
  ];

  for (const [type, opts] of otherKeyArgs) {
    const { publicKey } = generateKeyPairSync(type, {
      publicKeyEncoding: {
        format: 'pem',
        type: 'spki'
      },
      ...opts
    });

    assert.throws(() => {
      createPublicKey({
        key: publicKey,
        asymmetricKeyType: 'sm2'
      })
    }, {
      code: 'ERR_CRYPTO_INCOMPATIBLE_KEY',
      name: 'Error'
    });
  }
}

{
  // When importing key material, it must be possible to specify the
  // asymmetricKeyType since the ASN.1 key material itself does not distinguish
  // between EC using the SM2 curve and SM2.

  const ecPrivateExport = ecdsaPrivateKey.export({
    format: 'pem',
    type: 'pkcs8'
  });

  const ecPublicExport = ecdsaPublicKey.export({
    format: 'pem',
    type: 'spki'
  });

  const ecPrivateAsSm2Private = createPrivateKey({
    key: ecPrivateExport,
    asymmetricKeyType: 'sm2'
  });

  const ecPrivateAsSm2Public = createPublicKey({
    key: ecPrivateExport,
    asymmetricKeyType: 'sm2'
  });

  const ecPublicAsSm2Public = createPublicKey({
    key: ecPublicExport,
    asymmetricKeyType: 'sm2'
  });


  assert.strictEqual(ecdsaPrivateKey.asymmetricKeyType, 'ec');
  assert.strictEqual(ecPrivateAsSm2Private.asymmetricKeyType, 'sm2');
  assert.strictEqual(ecPrivateAsSm2Public.asymmetricKeyType, 'sm2');
  assert.strictEqual(ecPublicAsSm2Public.asymmetricKeyType, 'sm2');


  const ecPrivateAsSm2PrivateExport = ecPrivateAsSm2Private.export({
    format: 'pem',
    type: 'pkcs8'
  });

  const ecPrivateAsSm2PublicExport = ecPrivateAsSm2Public.export({
    format: 'pem',
    type: 'spki'
  });

  const ecPublicAsSm2PublicExport = ecPublicAsSm2Public.export({
    format: 'pem',
    type: 'spki'
  });

  // The exported PEM string is exactly the same for EC and SM2.
  assert.strictEqual(ecPrivateAsSm2PrivateExport, ecPrivateExport);
  for (const e of [ecPrivateAsSm2PublicExport, ecPublicAsSm2PublicExport]) {
    assert.strictEqual(e, ecPublicExport);
  }
}
