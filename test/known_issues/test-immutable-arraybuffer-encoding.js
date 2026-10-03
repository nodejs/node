// Flags: --js-immutable-arraybuffer
'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');

for (const [name, input] of [['short', 'hello'], ['long', 'hello'.repeat(16)]]) {
  test(`TextEncoder.encodeInto with ${name} input preserves immutable bytes`, () => {
    const expected = Array(128).fill(0xaa);
    const ab = Uint8Array.from(expected).buffer.transferToImmutable();
    const destination = new Uint8Array(ab);

    // The short and long inputs exercise different native encoding paths.
    // The error behavior for immutable backing storage is not defined yet.
    // eslint-disable-next-line no-restricted-syntax
    assert.throws(() => {
      new TextEncoder().encodeInto(input, destination);
    });

    assert.deepStrictEqual(Array.from(new Uint8Array(ab)), expected);
  });
}
