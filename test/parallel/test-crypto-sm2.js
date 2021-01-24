'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPairSync
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
