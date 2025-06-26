'use strict';
require('../common');
const assert = require('assert').strict;
const test = require('node:test');
const { DecompressionStream } = require('stream/web');

async function expectTypeError(promise) {
  let threw = false;
  try {
    await promise;
  } catch (err) {
    threw = true;
    assert(err instanceof TypeError, `Expected TypeError, got ${err}`);
  }
  assert(threw, 'Expected promise to reject');
}

test('DecompressStream deflat emits error on trailing data', async () => {
  const valid = new Uint8Array([120, 156, 75, 4, 0, 0, 98, 0, 98]); // deflate('a')
  const empty = new Uint8Array(1);
  const invalid = new Uint8Array([...valid, ...empty]);
  const double = new Uint8Array([...valid, ...valid]);

  for (const chunk of [[invalid], [valid, empty], [valid, valid], [valid, double]]) {
    await expectTypeError(
      Array.fromAsync(
        new Blob([chunk]).stream().pipeThrough(new DecompressionStream('deflate'))
      )
    );
  }
});

test('DecompressStream gzip emits error on trailing data', async () => {
  const valid = new Uint8Array([31, 139, 8, 0, 0, 0, 0, 0, 0, 19, 75, 4,
                                0, 67, 190, 183, 232, 1, 0, 0, 0]); // gzip('a')
  const empty = new Uint8Array(1);
  const invalid = new Uint8Array([...valid, ...empty]);
  const double = new Uint8Array([...valid, ...valid]);
  for (const chunk of [[invalid], [valid, empty], [valid, valid], [double]]) {
    await expectTypeError(
      Array.fromAsync(
        new Blob([chunk]).stream().pipeThrough(new DecompressionStream('gzip'))
      )
    );
  }
});
