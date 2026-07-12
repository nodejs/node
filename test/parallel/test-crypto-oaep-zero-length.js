'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const crypto = require('crypto');

const { subtle } = globalThis.crypto;

// Regression test for https://github.com/nodejs/node/issues/57553.
{
  const privateKey = crypto.createPrivateKey(fixtures.readKey('rsa_private.pem', 'ascii'));
  const publicKey = crypto.createPublicKey(fixtures.readKey('rsa_public.pem', 'ascii'));

  const data = Buffer.alloc(0);
  {

    const ciphertext = crypto.publicEncrypt({
      padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
      key: publicKey,
    }, data);

    const plaintext = crypto.privateDecrypt({
      padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
      key: privateKey
    }, ciphertext);

    assert.deepStrictEqual(plaintext, data);
  }

  {
    const ciphertext = crypto.publicEncrypt(publicKey, data);
    const plaintext = crypto.privateDecrypt(privateKey, ciphertext);

    assert.deepStrictEqual(plaintext, data);
  }

  {
    (async () => {
      const pkcs8 = privateKey.export({ format: 'der', type: 'pkcs8' });
      const spki = publicKey.export({ format: 'der', type: 'spki' });

      const kp = {
        privateKey: await subtle.importKey('pkcs8', pkcs8, { name: 'RSA-OAEP', hash: 'SHA-1' }, false, ['decrypt']),
        publicKey: await subtle.importKey('spki', spki, { name: 'RSA-OAEP', hash: 'SHA-1' }, false, ['encrypt']),
      };

      const ciphertext = await subtle.encrypt('RSA-OAEP', kp.publicKey, data);
      const plaintext = await subtle.decrypt('RSA-OAEP', kp.privateKey, ciphertext);
      assert.deepStrictEqual(plaintext, data.buffer);
    })().then(common.mustCall());
  }
}
