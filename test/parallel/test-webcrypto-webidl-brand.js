// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;
const { CryptoKey } = require('internal/crypto/keys');
const { converters } = require('internal/crypto/webidl');

async function main() {
  const bytes = new Uint8Array(16);
  const key = await subtle.importKey('raw', bytes, 'AES-GCM', true, ['encrypt']);

  for (const value of [
    { __proto__: CryptoKey.prototype },
    { __proto__: key },
    Object.create(CryptoKey.prototype, Object.getOwnPropertyDescriptors(key)),
    new Proxy(key, {}),
  ]) {
    assert.throws(() => converters.CryptoKey(value), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
    });
    await assert.rejects(subtle.exportKey('raw', value), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }

  Object.setPrototypeOf(key, null);
  assert.strictEqual(converters.CryptoKey(key), key);
  assert.deepStrictEqual(new Uint8Array(await subtle.exportKey('raw', key)), bytes);
}

main().then(common.mustCall());
