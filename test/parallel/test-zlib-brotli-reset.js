'use strict';

require('../common');
const assert = require('assert');
const { finished } = require('stream/promises');
const test = require('node:test');
const zlib = require('zlib');

const dictionary = Buffer.from(
  'Lorem ipsum dolor sit amet, consectetur adipiscing elit. ' +
  'Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.',
);
const input = Buffer.from(
  'Lorem ipsum dolor sit amet, consectetur adipiscing elit. '.repeat(100),
);

async function collect(stream, ...data) {
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));
  for (let i = 0; i < data.length - 1; i++) {
    stream.write(data[i]);
  }
  stream.end(data[data.length - 1]);
  await finished(stream);
  return Buffer.concat(chunks);
}

test('BrotliCompress reset preserves its initial options', async () => {
  const options = {
    dictionary,
    params: {
      [zlib.constants.BROTLI_PARAM_QUALITY]: 0,
    },
  };
  const expected = await collect(zlib.createBrotliCompress(options), input);
  const reset = zlib.createBrotliCompress(options);
  reset.reset();

  assert.deepStrictEqual(await collect(reset, input), expected);
});

test('BrotliDecompress reset preserves its dictionary', async () => {
  const compressed = zlib.brotliCompressSync(input, { dictionary });
  const decompress = zlib.createBrotliDecompress({ dictionary });
  decompress.reset();

  assert.deepStrictEqual(await collect(decompress, compressed), input);
});
