'use strict';

// Regression test for https://github.com/nodejs/node/issues/66078: ending a
// ZstdCompress stream with writes still queued appended an empty frame.

const common = require('../common');
const assert = require('assert');
const { finished } = require('stream/promises');
const zlib = require('zlib');

const kMagic = Buffer.from([0x28, 0xb5, 0x2f, 0xfd]);

function countFrames(buffer) {
  let count = 0;
  for (let i = buffer.indexOf(kMagic); i !== -1; i = buffer.indexOf(kMagic, i + 1)) {
    count++;
  }
  return count;
}

async function compress(use) {
  const stream = zlib.createZstdCompress();
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));
  await use(stream);
  stream.end();
  await finished(stream);
  return Buffer.concat(chunks);
}

(async () => {
  const expected = await compress((stream) => stream.write('hello world'));
  assert.strictEqual(countFrames(expected), 1);

  assert.deepStrictEqual(await compress((stream) => {
    stream.write('hello ');
    stream.write('world');
  }), expected);

  assert.deepStrictEqual(await compress((stream) => {
    stream.write('hello world');
    stream.flush(zlib.constants.ZSTD_e_end);
  }), expected);

  // The empty flush has to complete before end(), which would otherwise
  // promote it to ZSTD_e_end.
  assert.deepStrictEqual(await compress((stream) => {
    stream.write('hello world');
    stream.flush(zlib.constants.ZSTD_e_end);
    return new Promise((resolve) => stream.flush(resolve));
  }), expected);

  const empty = await compress(() => {});
  assert.strictEqual(countFrames(empty), 1);
  assert.strictEqual(zlib.zstdDecompressSync(empty).length, 0);

  const twoFrames = await compress((stream) => {
    stream.write('hello ');
    stream.flush(zlib.constants.ZSTD_e_end);
    stream.write('world');
  });
  assert.strictEqual(countFrames(twoFrames), 2);
  assert.strictEqual(zlib.zstdDecompressSync(twoFrames).toString(), 'hello world');
})().then(common.mustCall());
