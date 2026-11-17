'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL } = require('../common/crypto');
if (!isBoringSSL)
  common.skip('BoringSSL-only test');

const assert = require('assert');
const {
  constants,
  createPrivateKey,
  createPublicKey,
  createSign,
  createVerify,
  sign,
  subtle,
  verify,
} = require('crypto');
const { promisify } = require('util');
const signAsync = promisify(sign);
const verifyAsync = promisify(verify);
// BoringSSL exposes SHA-3 digests, but its PKCS1v1.5 DigestInfo table and ECDSA
// digest restrictions still reject them. Use valid independent signatures so
// these assertions fail when support lands and the fixture skips can be removed.
const vectors = [
  ...require('../fixtures/crypto/rsa_pkcs')({ includeUnsupported: true }),
  ...require('../fixtures/crypto/ecdsa')({ includeUnsupported: true }),
].filter(({ hash }) => hash.startsWith('SHA3-'));
assert.strictEqual(vectors.length, 12);

async function test(vector) {
  const {
    hash,
    namedCurve,
    plaintext,
    privateKeyBuffer,
    publicKeyBuffer,
    signature,
  } = vector;
  const isECDSA = vector.name === 'ECDSA';
  const name = isECDSA ? vector.name : vector.algorithm.name;
  const context = `${name} ${namedCurve ?? ''} ${hash}`;
  const error = {
    code: isECDSA ? 'ERR_OSSL_EVP_INVALID_DIGEST_TYPE' :
      'ERR_OSSL_RSA_UNKNOWN_ALGORITHM_TYPE',
  };
  const privateKey = createPrivateKey({
    key: privateKeyBuffer, format: 'der', type: 'pkcs8',
  });
  const publicKey = createPublicKey({
    key: publicKeyBuffer, format: 'der', type: 'spki',
  });
  const options = isECDSA ? { dsaEncoding: 'ieee-p1363' } :
    { padding: constants.RSA_PKCS1_PADDING };
  const signingKey = { key: privateKey, ...options };
  const verificationKey = { key: publicKey, ...options };

  assert.throws(() => sign(hash, plaintext, signingKey), error, context);
  await assert.rejects(signAsync(hash, plaintext, signingKey), error, context);
  assert.throws(() => createSign(hash).update(plaintext).sign(signingKey),
                error, context);

  if (isECDSA) {
    assert.throws(() => verify(hash, plaintext, verificationKey, signature),
                  error, context);
    await assert.rejects(verifyAsync(hash, plaintext, verificationKey, signature),
                         error, context);
  } else {
    assert.strictEqual(verify(hash, plaintext, verificationKey, signature), false,
                       context);
    assert.strictEqual(
      await verifyAsync(hash, plaintext, verificationKey, signature), false,
      context);
  }
  assert.strictEqual(createVerify(hash).update(plaintext)
    .verify(verificationKey, signature), false, context);

  const unsupportedHash = { name: 'NotSupportedError' };
  if (!isECDSA) {
    const algorithm = { name, hash };
    await assert.rejects(subtle.importKey(
      'pkcs8', privateKeyBuffer, algorithm, false, ['sign']),
                         unsupportedHash, context);
    await assert.rejects(subtle.importKey(
      'spki', publicKeyBuffer, algorithm, false, ['verify']),
                         unsupportedHash, context);
    return;
  }

  const [webPrivateKey, webPublicKey] = await Promise.all([
    subtle.importKey('pkcs8', privateKeyBuffer, { name, namedCurve }, false, ['sign']),
    subtle.importKey('spki', publicKeyBuffer, { name, namedCurve }, false, ['verify']),
  ]);
  await assert.rejects(subtle.sign({ name, hash }, webPrivateKey, plaintext),
                       unsupportedHash, context);
  await assert.rejects(subtle.verify({ name, hash }, webPublicKey, signature,
                                     plaintext), unsupportedHash, context);
}

for (const vector of vectors)
  test(vector).then(common.mustCall());
