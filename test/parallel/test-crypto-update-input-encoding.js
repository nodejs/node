'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');

const privateKey = fixtures.readKey('rsa_private.pem');
const publicKey = fixtures.readKey('rsa_public.pem');
const data = 'caf\u00e9';
const bytes = Buffer.from(data);
const signature = crypto.sign('sha256', bytes, privateKey);

const factories = [
  () => crypto.createHash('sha256'),
  () => crypto.createHmac('sha256', 'key'),
  () => crypto.createSign('sha256'),
  () => crypto.createVerify('sha256'),
  () => crypto.createCipheriv('aes-128-ctr', Buffer.alloc(16), Buffer.alloc(16)),
  () => crypto.createDecipheriv('aes-128-ctr', Buffer.alloc(16), Buffer.alloc(16)),
];

function finish(instance, data, encoding) {
  const result = instance.update(data, encoding);
  if (instance instanceof crypto.Hash || instance instanceof crypto.Hmac)
    return instance.digest();
  if (instance instanceof crypto.Sign)
    return instance.sign(privateKey);
  if (instance instanceof crypto.Verify)
    return instance.verify(publicKey, signature);
  return Buffer.concat([result, instance.final()]);
}

for (const create of factories) {
  const expected = finish(create(), bytes);
  for (const encoding of ['bad', 'buffer']) {
    const instance = create();
    assert.throws(() => instance.update(data, encoding), {
      code: 'ERR_UNKNOWN_ENCODING',
      message: `Unknown encoding: ${encoding}`,
    });
    // Rejected input must not change the operation's state.
    assert.deepStrictEqual(finish(instance, bytes), expected);
  }
  for (const encoding of [undefined, null, '', 'utf8', 'UTF-8']) {
    assert.deepStrictEqual(finish(create(), data, encoding), expected);
  }
  assert.deepStrictEqual(finish(create(), bytes.toString('hex'), 'hex'), expected);
  for (const input of [
    bytes,
    new Uint8Array(bytes),
    new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength),
  ]) {
    assert.deepStrictEqual(finish(create(), input, 'bad'), expected);
  }
  assert.throws(() => create().update('a', 'hex'), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}
