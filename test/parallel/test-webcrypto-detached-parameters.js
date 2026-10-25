'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

function detached(kind) {
  const buffer = new ArrayBuffer(16);
  const value = kind === 'ArrayBuffer' ? buffer :
    kind === 'DataView' ? new DataView(buffer) : new Uint8Array(buffer);
  buffer.transfer();
  return value;
}

(async () => {
  const empty = new Uint8Array(0);
  const input = new Uint8Array(16);
  const key = await subtle.importKey('raw', input, 'HKDF', false, ['deriveBits']);
  const algorithm = { name: 'HKDF', hash: 'SHA-256', salt: empty, info: empty };
  const expected = await subtle.deriveBits(algorithm, key, 256);
  const digest = await subtle.digest('SHA-256', empty);
  for (const kind of ['ArrayBuffer', 'Uint8Array', 'DataView']) {
    for (const member of ['salt', 'info']) {
      assert.deepStrictEqual(await subtle.deriveBits({
        ...algorithm, [member]: detached(kind),
      }, key, 256), expected);
    }
    assert.deepStrictEqual(await subtle.digest('SHA-256', detached(kind)), digest);
  }
})().then(common.mustCall());
