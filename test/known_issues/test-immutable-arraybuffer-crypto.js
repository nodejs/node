// Flags: --js-immutable-arraybuffer
'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { test } = require('node:test');

const cases = [
  ['randomFillSync with an ArrayBuffer', (buffer) => {
    crypto.randomFillSync(buffer);
  }],
  ['randomFill with a DataView', (buffer) => {
    return new Promise((resolve, reject) => {
      crypto.randomFill(new DataView(buffer), (err) => {
        if (err) {
          reject(err);
          return;
        }
        resolve();
      });
    });
  }],
  ['getRandomValues with a Uint8Array', (buffer) => {
    crypto.getRandomValues(new Uint8Array(buffer));
  }],
  ['global crypto.getRandomValues with a Buffer', (buffer) => {
    globalThis.crypto.getRandomValues(Buffer.from(buffer));
  }],
];

for (const [name, fill] of cases) {
  test(name, async () => {
    const buffer = new ArrayBuffer(32).transferToImmutable();
    const bytes = new Uint8Array(buffer);
    const original = Array.from(bytes);

    try {
      await fill(buffer);
    } catch (err) {
      if (!(err instanceof TypeError))
        throw err;
    }

    assert.strictEqual(buffer.immutable, true);
    assert.deepStrictEqual(Array.from(bytes), original);
  });
}
