'use strict';

require('../common');
const assert = require('assert');
const test = require('node:test');
const { finished } = require('stream/promises');
const zlib = require('zlib');

const trailingJunkError = {
  code: 'ERR_TRAILING_JUNK_AFTER_STREAM_END',
  name: 'TypeError',
};

function callAsync(fn, input, options) {
  return new Promise((resolve, reject) => {
    fn(input, options, (err, result) => {
      if (err) {
        reject(err);
      } else {
        resolve(result);
      }
    });
  });
}

async function collect(stream, input) {
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));
  stream.end(input);
  await finished(stream);
  return Buffer.concat(chunks);
}

const cases = [
  {
    label: 'inflate',
    compress: zlib.deflateSync,
    decompress: zlib.inflate,
    decompressSync: zlib.inflateSync,
    createDecompress: zlib.createInflate,
    defaultOutput: 'a',
  },
  {
    label: 'inflateRaw',
    compress: zlib.deflateRawSync,
    decompress: zlib.inflateRaw,
    decompressSync: zlib.inflateRawSync,
    createDecompress: zlib.createInflateRaw,
    defaultOutput: 'a',
  },
  {
    label: 'gunzip',
    compress: zlib.gzipSync,
    decompress: zlib.gunzip,
    decompressSync: zlib.gunzipSync,
    createDecompress: zlib.createGunzip,
    defaultOutput: 'aa',
  },
  {
    label: 'unzip',
    compress: zlib.gzipSync,
    decompress: zlib.unzip,
    decompressSync: zlib.unzipSync,
    createDecompress: zlib.createUnzip,
    defaultOutput: 'aa',
  },
  {
    label: 'brotli',
    compress: zlib.brotliCompressSync,
    decompress: zlib.brotliDecompress,
    decompressSync: zlib.brotliDecompressSync,
    createDecompress: zlib.createBrotliDecompress,
    defaultOutput: 'a',
  },
  {
    label: 'zstd',
    compress: zlib.zstdCompressSync,
    decompress: zlib.zstdDecompress,
    decompressSync: zlib.zstdDecompressSync,
    createDecompress: zlib.createZstdDecompress,
    defaultOutput: 'a',
  },
];

for (const {
  label,
  compress,
  decompress,
  decompressSync,
  createDecompress,
  defaultOutput,
} of cases) {
  test(`rejectGarbageAfterEnd rejects trailing input for ${label}`, async () => {
    const compressed = compress(Buffer.from('a'));
    const withTrailingInput = Buffer.concat([compressed, compressed]);

    assert.strictEqual(decompressSync(withTrailingInput).toString(), defaultOutput);
    assert.strictEqual(
      (await callAsync(decompress, withTrailingInput)).toString(),
      defaultOutput,
    );
    assert.strictEqual(
      (await collect(createDecompress(), withTrailingInput)).toString(),
      defaultOutput,
    );

    assert.throws(
      () => decompressSync(withTrailingInput, { rejectGarbageAfterEnd: true }),
      trailingJunkError,
    );
    await assert.rejects(
      callAsync(decompress, withTrailingInput, { rejectGarbageAfterEnd: true }),
      trailingJunkError,
    );
    await assert.rejects(
      collect(
        createDecompress({ rejectGarbageAfterEnd: true }),
        withTrailingInput,
      ),
      trailingJunkError,
    );
  });
}

test('rejectGarbageAfterEnd must be a boolean', () => {
  const compressed = zlib.deflateSync(Buffer.from('a'));

  for (const value of [1, 'true', null]) {
    assert.throws(
      () => zlib.inflateSync(compressed, { rejectGarbageAfterEnd: value }),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      },
    );
    assert.throws(
      () => zlib.createInflate({ rejectGarbageAfterEnd: value }),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      },
    );
  }
});
