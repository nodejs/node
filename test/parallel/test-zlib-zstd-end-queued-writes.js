'use strict';

require('../common');
const assert = require('assert');
const { finished } = require('stream/promises');
const test = require('node:test');
const zlib = require('zlib');

async function compress(write) {
  const stream = zlib.createZstdCompress();
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));
  write(stream);
  stream.end();
  await finished(stream);
  return Buffer.concat(chunks);
}

test('ZstdCompress ends the frame once when writes are queued', async () => {
  const expected = await compress((stream) => stream.write('hello world'));
  assert.deepStrictEqual(zlib.zstdDecompressSync(expected),
                         Buffer.from('hello world'));

  assert.deepStrictEqual(await compress((stream) => {
    stream.write('hello ');
    stream.write('world');
  }), expected);
});

test('ZstdCompress ends the frame once after an explicit end flush', async () => {
  const expected = await compress((stream) => stream.write('hello world'));

  assert.deepStrictEqual(await compress((stream) => {
    stream.write('hello world');
    stream.flush(zlib.constants.ZSTD_e_end);
  }), expected);
});
