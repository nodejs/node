'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const size = common.hasFipsCrypto || common.hasOpenSSL3 ? 1024 : 256;
const dh1 = crypto.createDiffieHellman(size);
const p1 = dh1.getPrime('buffer');
const dh2 = crypto.createDiffieHellman(p1, 'buffer');
const key1 = dh1.generateKeys();
const key2 = dh2.generateKeys('hex');
const secret1 = dh1.computeSecret(key2, 'hex', 'base64');
const secret2 = dh2.computeSecret(key1, 'latin1', 'buffer');

// Test Diffie-Hellman with two parties sharing a secret,
// using various encodings as we go along
assert.strictEqual(secret2.toString('base64'), secret1);
assert.strictEqual(dh1.verifyError, 0);
assert.strictEqual(dh2.verifyError, 0);

// https://github.com/nodejs/node/issues/32738
// XXX(bnoordhuis) validateInt32() throwing ERR_OUT_OF_RANGE and RangeError
// instead of ERR_INVALID_ARG_TYPE and TypeError is questionable, IMO.
assert.throws(() => crypto.createDiffieHellman(13.37), {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "sizeOrKey" is out of range. ' +
           'It must be an integer. Received 13.37',
});

assert.throws(() => crypto.createDiffieHellman('abcdef', 13.37), {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "generator" is out of range. ' +
           'It must be an integer. Received 13.37',
});

for (const bits of [-1, 0, 1]) {
  if (common.hasOpenSSL3) {
    assert.throws(() => crypto.createDiffieHellman(bits), {
      code: 'ERR_OSSL_DH_MODULUS_TOO_SMALL',
      name: 'Error',
      message: /modulus too small/,
    });
  } else {
    assert.throws(() => crypto.createDiffieHellman(bits), {
      code: 'ERR_OSSL_BN_BITS_TOO_SMALL',
      name: 'Error',
      message: /bits too small/,
    });
  }
}

// Through a fluke of history, g=0 defaults to DH_GENERATOR (2).
{
  const g = 0;
  crypto.createDiffieHellman('abcdef', g);
  crypto.createDiffieHellman('abcdef', 'hex', g);
}

for (const g of [-1, 1]) {
  const ex = {
    code: 'ERR_OSSL_DH_BAD_GENERATOR',
    name: 'Error',
    message: /bad generator/,
  };
  assert.throws(() => crypto.createDiffieHellman('abcdef', g), ex);
  assert.throws(() => crypto.createDiffieHellman('abcdef', 'hex', g), ex);
}

crypto.createDiffieHellman('abcdef', Buffer.from([2]));  // OK

for (const g of [Buffer.from([]),
                 Buffer.from([0]),
                 Buffer.from([1])]) {
  const ex = {
    code: 'ERR_OSSL_DH_BAD_GENERATOR',
    name: 'Error',
    message: /bad generator/,
  };
  assert.throws(() => crypto.createDiffieHellman('abcdef', g), ex);
  assert.throws(() => crypto.createDiffieHellman('abcdef', 'hex', g), ex);
}

[
  [0x1, 0x2],
  () => { },
  /abc/,
  {},
].forEach((input) => {
  assert.throws(
    () => crypto.createDiffieHellman(input),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
});

// Create "another dh1" using generated keys from dh1,
// and compute secret again
const dh3 = crypto.createDiffieHellman(p1, 'buffer');
const privkey1 = dh1.getPrivateKey();
dh3.setPublicKey(key1);
dh3.setPrivateKey(privkey1);

assert.deepStrictEqual(dh1.getPrime(), dh3.getPrime());
assert.deepStrictEqual(dh1.getGenerator(), dh3.getGenerator());
assert.deepStrictEqual(dh1.getPublicKey(), dh3.getPublicKey());
assert.deepStrictEqual(dh1.getPrivateKey(), dh3.getPrivateKey());
assert.strictEqual(dh3.verifyError, 0);

const secret3 = dh3.computeSecret(key2, 'hex', 'base64');

assert.strictEqual(secret1, secret3);

// computeSecret works without a public key set at all.
const dh4 = crypto.createDiffieHellman(p1, 'buffer');
dh4.setPrivateKey(privkey1);

assert.deepStrictEqual(dh1.getPrime(), dh4.getPrime());
assert.deepStrictEqual(dh1.getGenerator(), dh4.getGenerator());
assert.deepStrictEqual(dh1.getPrivateKey(), dh4.getPrivateKey());
assert.strictEqual(dh4.verifyError, 0);

const secret4 = dh4.computeSecret(key2, 'hex', 'base64');

assert.strictEqual(secret1, secret4);

let wrongBlockLength;
if (common.hasOpenSSL3) {
  wrongBlockLength = {
    message: 'error:1C80006B:Provider routines::wrong final block length',
    code: 'ERR_OSSL_WRONG_FINAL_BLOCK_LENGTH',
    library: 'Provider routines',
    reason: 'wrong final block length'
  };
} else {
  wrongBlockLength = {
    message: 'error:0606506D:digital envelope' +
      ' routines:EVP_DecryptFinal_ex:wrong final block length',
    code: 'ERR_OSSL_EVP_WRONG_FINAL_BLOCK_LENGTH',
    library: 'digital envelope routines',
    reason: 'wrong final block length'
  };
}

// Run this one twice to make sure that the dh3 clears its error properly
{
  const c = crypto.createDecipheriv('aes-128-ecb', crypto.randomBytes(16), '');
  assert.throws(() => {
    c.final('utf8');
  }, wrongBlockLength);
}

{
  const c = crypto.createDecipheriv('aes-128-ecb', crypto.randomBytes(16), '');
  assert.throws(() => {
    c.final('utf8');
  }, wrongBlockLength);
}

assert.throws(() => {
  dh3.computeSecret('');
}, { message: common.hasOpenSSL3 ?
  'error:02800080:Diffie-Hellman routines::invalid secret' :
  'Supplied key is too small' });

// Invalid test: curve argument is undefined
assert.throws(
  () => crypto.createECDH(),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "curve" argument must be of type string. ' +
            'Received undefined'
  });

assert.throws(
  function() {
    crypto.getDiffieHellman('unknown-group');
  },
  {
    name: 'Error',
    code: 'ERR_CRYPTO_UNKNOWN_DH_GROUP',
    message: 'Unknown DH group'
  },
  'crypto.getDiffieHellman(\'unknown-group\') ' +
  'failed to throw the expected error.'
);

assert.throws(
  () => crypto.createDiffieHellman('', true),
  {
    code: 'ERR_INVALID_ARG_TYPE'
  }
);
[true, Symbol(), {}, () => {}, []].forEach((generator) => assert.throws(
  () => crypto.createDiffieHellman('', 'base64', generator),
  { code: 'ERR_INVALID_ARG_TYPE' }
));
