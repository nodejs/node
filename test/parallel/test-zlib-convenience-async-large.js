'use strict';
// The asynchronous convenience methods grow their output buffer while
// processing large inputs. Check that large results, custom chunkSize values
// and maxOutputLength behave exactly like the synchronous methods.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const zlib = require('zlib');

const { Z_MIN_CHUNK, BROTLI_PARAM_QUALITY } = zlib.constants;

// Text plus pseudo-random bytes, so that both the compressed and the
// decompressed output span many output buffers.
const random = Buffer.alloc(256 * 1024);
{
  let seed = 0x2545f491;
  for (let i = 0; i < random.length; i++) {
    seed ^= seed << 13;
    seed ^= seed >>> 17;
    seed ^= seed << 5;
    random[i] = seed & 0xff;
  }
}
const data = Buffer.concat([
  Buffer.alloc(2 * 1024 * 1024, fs.readFileSync(__filename)),
  random,
]);

// Keep brotli fast enough for a test.
const brotliOptions = { params: { [BROTLI_PARAM_QUALITY]: 4 } };
const pairs = [
  ['deflate', 'inflate'],
  ['gzip', 'gunzip'],
  ['gzip', 'unzip'],
  ['deflateRaw', 'inflateRaw'],
  ['brotliCompress', 'brotliDecompress', brotliOptions],
  ['zstdCompress', 'zstdDecompress'],
];

function callAsync(method, input, options) {
  return new Promise((resolve, reject) => {
    zlib[method](input, options, common.mustCall((err, result) => {
      if (err) reject(err);
      else resolve(result);
    }));
  });
}

(async () => {
  for (const [compress, decompress, options = {}] of pairs) {
    const label = `${compress}/${decompress}`;
    const compressed = await callAsync(compress, data, options);
    const expected = zlib[`${compress}Sync`](data, options);
    // zstdCompress() pledges the input size and ends the frame separately,
    // which yields different (not larger) frames than zstdCompressSync().
    if (compress !== 'zstdCompress') {
      assert.deepStrictEqual(compressed, expected, label);
    }
    assert.deepStrictEqual(await callAsync(decompress, compressed, {}), data,
                           label);

    for (const chunkSize of [Z_MIN_CHUNK, data.length + 1]) {
      assert.deepStrictEqual(
        await callAsync(decompress, compressed, { chunkSize }), data,
        `${label} chunkSize=${chunkSize}`);
    }

    for (const maxOutputLength of [data.length - 1, 100000, 1]) {
      await assert.rejects(
        callAsync(decompress, compressed, { maxOutputLength }), {
          name: 'RangeError',
          code: 'ERR_BUFFER_TOO_LARGE',
          message: `Cannot create a Buffer larger than ${maxOutputLength} bytes`,
        }, `${label} maxOutputLength=${maxOutputLength}`);
    }
    assert.deepStrictEqual(
      await callAsync(decompress, compressed,
                      { maxOutputLength: data.length }),
      data, label);
  }
})().then(common.mustCall());
