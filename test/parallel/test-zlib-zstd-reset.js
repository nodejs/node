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

test('ZstdCompress reset preserves its initial options', async () => {
  const options = {
    dictionary,
    pledgedSrcSize: input.length,
    params: {
      [zlib.constants.ZSTD_c_compressionLevel]: 19,
      [zlib.constants.ZSTD_c_checksumFlag]: 1,
    },
  };
  const expected = await collect(zlib.createZstdCompress(options), input);
  const reset = zlib.createZstdCompress(options);
  reset.reset();

  assert.deepStrictEqual(await collect(reset, input), expected);
});

test('ZstdDecompress reset preserves its dictionary', async () => {
  const compressed = zlib.zstdCompressSync(input, { dictionary });
  const decompress = zlib.createZstdDecompress({ dictionary });
  decompress.reset();

  assert.deepStrictEqual(await collect(decompress, compressed), input);
});

test('ZstdDecompress reset preserves its parameters', async () => {
  const compressed = await collect(zlib.createZstdCompress({
    params: {
      [zlib.constants.ZSTD_c_windowLog]: 11,
    },
  }), Buffer.alloc(2048), Buffer.alloc(2048));
  const decompress = zlib.createZstdDecompress({
    params: {
      [zlib.constants.ZSTD_d_windowLogMax]: 10,
    },
  });
  decompress.reset();

  await assert.rejects(collect(decompress, compressed), {
    code: 'ZSTD_error_frameParameter_windowTooLarge',
  });
});
