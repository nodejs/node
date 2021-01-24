'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPairSync,
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
