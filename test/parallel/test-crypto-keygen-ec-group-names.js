'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { generateKeyPair, generateKeyPairSync, sign, verify } = require('crypto');

const message = Buffer.from('EC group names');
for (const [alias, canonical] of [
  ['P-256', 'prime256v1'],
  ['P-384', 'secp384r1'],
  ['P-521', 'secp521r1'],
]) {
  for (const namedCurve of [alias, canonical]) {
    const check = common.mustCall((publicKey, privateKey) => {
      assert.deepStrictEqual(publicKey.asymmetricKeyDetails,
                             { namedCurve: canonical });
      assert.deepStrictEqual(privateKey.asymmetricKeyDetails,
                             { namedCurve: canonical });
      assert(verify('sha256', message, publicKey,
                    sign('sha256', message, privateKey)));
    }, 2);
    const { publicKey, privateKey } = generateKeyPairSync('ec', { namedCurve });
    check(publicKey, privateKey);
    generateKeyPair('ec', { namedCurve }, common.mustSucceed(check));
  }
}

for (const namedCurve of ['', 'node-test-unknown-curve']) {
  assert.throws(() => generateKeyPairSync('ec', { namedCurve }), {
    code: 'ERR_CRYPTO_INVALID_CURVE',
  });
  assert.throws(() => generateKeyPair('ec', { namedCurve }, common.mustNotCall()), {
    code: 'ERR_CRYPTO_INVALID_CURVE',
  });
}
