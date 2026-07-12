'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const crypto = require('crypto');

// Tests for CVE-2022-21449
// https://neilmadden.blog/2022/04/19/psychic-signatures-in-java/
// Dubbed "Psychic Signatures", these signatures bypassed the ECDSA signature
// verification implementation in Java in 15, 16, 17, and 18. OpenSSL is not
// (and was not) vulnerable so these are a precaution.

const vectors = {
  'ieee-p1363': [
    Buffer.from('0000000000000000000000000000000000000000000000000000000000000000' +
      '0000000000000000000000000000000000000000000000000000000000000000', 'hex'),
    Buffer.from('ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551' +
      'ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551', 'hex'),
  ],
  'der': [
    Buffer.from('3046022100' +
      '0000000000000000000000000000000000000000000000000000000000000000' +
      '022100' +
      '0000000000000000000000000000000000000000000000000000000000000000', 'hex'),
    Buffer.from('3046022100' +
      'ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551' +
      '022100' +
      'ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551', 'hex'),
  ],
};

const keyPair = crypto.generateKeyPairSync('ec', {
  namedCurve: 'P-256',
  publicKeyEncoding: {
    format: 'der',
    type: 'spki'
  },
});

const data = Buffer.from('Hello!');

for (const [encoding, signatures] of Object.entries(vectors)) {
  for (const signature of signatures) {
    const key = {
      key: keyPair.publicKey,
      format: 'der',
      type: 'spki',
      dsaEncoding: encoding,
    };

    // one-shot sync
    assert.strictEqual(
      crypto.verify(
        'sha256',
        data,
        key,
        signature,
      ),
      false,
    );

    // one-shot async
    crypto.verify(
      'sha256',
      data,
      key,
      signature,
      common.mustSucceed((verified) => assert.strictEqual(verified, false)),
    );

    // stream
    assert.strictEqual(
      crypto.createVerify('sha256')
        .update(data)
        .verify(key, signature),
      false,
    );

    // webcrypto
    globalThis.crypto.subtle.importKey(
      'spki',
      keyPair.publicKey,
      { name: 'ECDSA', namedCurve: 'P-256' },
      false,
      ['verify'],
    ).then((publicKey) => {
      return globalThis.crypto.subtle.verify(
        { name: 'ECDSA', hash: 'SHA-256' },
        publicKey,
        signature,
        data,
      );
    }).then(common.mustCall((verified) => {
      assert.strictEqual(verified, false);
    }));
  }
}
