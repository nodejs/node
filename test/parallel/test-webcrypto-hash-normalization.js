'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  const data = new Uint8Array(32);
  let reads = 0;
  const key = await subtle.importKey('raw', data, {
    name: 'HMAC',
    hash: {
      get name() {
        reads++;
        return reads === 1 ? 'SHA-384' : 'SHA-256';
      },
    },
  }, false, ['sign']);
  assert.strictEqual(reads, 1);
  assert.strictEqual(key.algorithm.hash.name, 'SHA-384');
  assert.strictEqual((await subtle.sign('HMAC', key, data)).byteLength, 48);

  const converted = await subtle.importKey('raw', data, {
    name: 'HMAC',
    hash: { name: { toString() { return 'SHA-256'; } } },
  }, false, ['sign']);
  assert.strictEqual(converted.algorithm.hash.name, 'SHA-256');

  await assert.rejects(subtle.importKey('raw', data, {
    name: 'HMAC', hash: {},
  }, false, ['sign']), TypeError);
  const cshakeAvailable = SubtleCrypto.supports('digest', {
    name: 'cSHAKE128', outputLength: 256,
  });
  await assert.rejects(subtle.importKey('raw', data, {
    name: 'HMAC', hash: 'cSHAKE128',
  }, false, ['sign']), cshakeAvailable ? TypeError : { name: 'NotSupportedError' });
  await assert.rejects(subtle.importKey('raw', data, {
    name: 'HMAC', hash: 'MD5', length: -1,
  }, false, ['sign']), TypeError);
  await assert.rejects(subtle.importKey('raw', data, {
    name: 'HMAC', hash: { name: 'cSHAKE128', outputLength: 256 },
  }, false, ['sign']), { name: 'NotSupportedError' });
})().then(common.mustCall());
