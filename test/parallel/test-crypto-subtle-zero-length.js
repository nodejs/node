'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto').webcrypto;

(async () => {
  const k = await crypto.subtle.importKey(
    'raw',
    new Uint8Array(32),
    { name: 'AES-GCM' },
    false,
    [ 'encrypt', 'decrypt' ]);
  assert(k instanceof crypto.CryptoKey);

  const e = await crypto.subtle.encrypt({
    name: 'AES-GCM',
    iv: new Uint8Array(12),
  }, k, new Uint8Array(0));
  assert(e instanceof ArrayBuffer);
  assert.deepStrictEqual(
    Buffer.from(e),
    Buffer.from([
      0x53, 0x0f, 0x8a, 0xfb, 0xc7, 0x45, 0x36, 0xb9,
      0xa9, 0x63, 0xb4, 0xf1, 0xc4, 0xcb, 0x73, 0x8b ]));

  const v = await crypto.subtle.decrypt({
    name: 'AES-GCM',
    iv: new Uint8Array(12),
  }, k, e);
  assert(v instanceof ArrayBuffer);
  assert.strictEqual(v.byteLength, 0);
})().then(common.mustCall()).catch((e) => {
  assert.ifError(e);
});
