'use strict';
// Tests the contract of the asynchronous convenience methods
// (zlib.deflate(), zlib.gunzip(), zlib.brotliCompress(), ...).

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const fs = require('fs');
const { promisify } = require('util');
const zlib = require('zlib');

const {
  Z_SYNC_FLUSH,
  Z_FULL_FLUSH,
  Z_BUF_ERROR,
  Z_DATA_ERROR,
  Z_NEED_DICT,
  Z_MIN_CHUNK,
  Z_DEFAULT_CHUNK,
  BROTLI_OPERATION_FLUSH,
  ZSTD_e_flush,
} = zlib.constants;

const pairs = [
  { compress: 'deflate', decompress: 'inflate', engine: 'Inflate' },
  { compress: 'gzip', decompress: 'gunzip', engine: 'Gunzip' },
  { compress: 'gzip', decompress: 'unzip', engine: 'Unzip' },
  { compress: 'deflateRaw', decompress: 'inflateRaw', engine: 'InflateRaw' },
  {
    compress: 'brotliCompress',
    decompress: 'brotliDecompress',
    engine: 'BrotliDecompress',
    partialFlush: BROTLI_OPERATION_FLUSH,
  },
  {
    compress: 'zstdCompress',
    decompress: 'zstdDecompress',
    engine: 'ZstdDecompress',
    partialFlush: ZSTD_e_flush,
  },
];

// Calls an asynchronous convenience method and resolves with its result.
// The callback must be called exactly once, and never synchronously.
function callAsync(method, input, options) {
  return new Promise((resolve, reject) => {
    let returned = false;
    const callback = common.mustCall((err, result) => {
      assert.strictEqual(returned, true,
                         `${method}() called its callback synchronously`);
      if (err) reject(err);
      else resolve(result);
    });
    if (options === undefined) {
      zlib[method](input, callback);
    } else {
      zlib[method](input, options, callback);
    }
    returned = true;
  });
}

// Text plus pseudo-random bytes, large enough that the output of every
// algorithm spans several Z_DEFAULT_CHUNK-sized output buffers.
const text = fs.readFileSync(__filename);
const random = Buffer.alloc(3 * Z_DEFAULT_CHUNK);
{
  let seed = 0x9e3779b9;
  for (let i = 0; i < random.length; i++) {
    seed ^= seed << 13;
    seed ^= seed >>> 17;
    seed ^= seed << 5;
    random[i] = seed & 0xff;
  }
}
const data = Buffer.concat([text, random, Buffer.alloc(8 * Z_DEFAULT_CHUNK, 'a')]);
const shortString = 'blah'.repeat(8);

async function testRoundTrip({ compress, decompress, partialFlush }) {
  const syncCompress = `${compress}Sync`;
  const syncDecompress = `${decompress}Sync`;

  // Every documented input type.
  const inputs = [
    ['string', shortString, Buffer.from(shortString)],
    ['long string', text.toString(), text],
    ['Buffer', data, data],
    ['large ArrayBuffer', new Uint8Array(data).buffer, data],
    ...common.getBufferSources(Buffer.from(shortString)).map((view) => [
      view[Symbol.toStringTag], view, Buffer.from(shortString),
    ]),
  ];

  for (const [type, input, expected] of inputs) {
    const label = `${compress}/${decompress} with ${type}`;
    const compressed = await callAsync(compress, input);
    assert.ok(Buffer.isBuffer(compressed), label);
    // zstdCompress() pledges the input size and ends the frame separately,
    // which yields different (not larger) frames than zstdCompressSync().
    if (compress !== 'zstdCompress') {
      assert.deepStrictEqual(compressed, zlib[syncCompress](input), label);
    }
    // Interoperable with the synchronous API in both directions.
    assert.deepStrictEqual(zlib[syncDecompress](compressed), expected, label);
    const decompressed =
      await callAsync(decompress, zlib[syncCompress](input));
    assert.ok(Buffer.isBuffer(decompressed), label);
    assert.deepStrictEqual(decompressed, expected, label);
    assert.deepStrictEqual(await callAsync(decompress, compressed), expected,
                           label);
  }

  // A minimal chunkSize makes the work for one call span many output
  // buffers.
  const options = { chunkSize: Z_MIN_CHUNK };
  const compressed = await callAsync(compress, data, options);
  if (compress !== 'zstdCompress') {
    assert.deepStrictEqual(compressed, zlib[syncCompress](data, options));
  }
  assert.deepStrictEqual(await callAsync(decompress, compressed, options),
                         data);

  // Empty input.
  const empty = await callAsync(compress, Buffer.alloc(0));
  assert.deepStrictEqual(await callAsync(decompress, empty), Buffer.alloc(0));
  assert.deepStrictEqual(
    await callAsync(decompress, Buffer.alloc(0),
                    { finishFlush: partialFlush ?? Z_SYNC_FLUSH }),
    Buffer.alloc(0));
}

async function testErrors({ compress, decompress, partialFlush }) {
  const compressed = zlib[`${compress}Sync`](data);

  // Truncated input.
  const truncated = compressed.subarray(0, compressed.length >> 1);
  await assert.rejects(callAsync(decompress, truncated), {
    name: 'Error',
    code: 'Z_BUF_ERROR',
    errno: Z_BUF_ERROR,
    message: 'unexpected end of file',
  });

  // A non-finalizing finishFlush returns as much as can be decompressed.
  const partial = await callAsync(decompress, truncated, {
    finishFlush: partialFlush ?? Z_SYNC_FLUSH,
  });
  // Zstd only emits whole blocks, and this input is truncated inside the
  // first one.
  if (decompress !== 'zstdDecompress') assert.ok(partial.length > 0);
  assert.deepStrictEqual(partial, data.subarray(0, partial.length));

  // maxOutputLength applies to the output of the whole call.
  await assert.rejects(callAsync(decompress, compressed,
                                 { maxOutputLength: data.length - 1 }), {
    name: 'RangeError',
    code: 'ERR_BUFFER_TOO_LARGE',
    message: `Cannot create a Buffer larger than ${data.length - 1} bytes`,
  });
  assert.deepStrictEqual(
    await callAsync(decompress, compressed, { maxOutputLength: data.length }),
    data);
  await assert.rejects(callAsync(compress, data, { maxOutputLength: 64 }), {
    name: 'RangeError',
    code: 'ERR_BUFFER_TOO_LARGE',
    message: 'Cannot create a Buffer larger than 64 bytes',
  });
}

async function testInfo({ compress, decompress, engine }) {
  const compressed = zlib[`${compress}Sync`](data);
  const result = await callAsync(decompress, compressed, { info: true });
  assert.deepStrictEqual(result.buffer, data);
  assert.ok(result.engine instanceof zlib[engine]);
  assert.strictEqual(result.engine.bytesWritten, compressed.length);
}

async function testTrailingGarbage({ compress, decompress }) {
  const compressed = zlib[`${compress}Sync`](data);
  const withJunk = Buffer.concat([compressed, Buffer.from('junk')]);
  await assert.rejects(
    callAsync(decompress, withJunk, { rejectGarbageAfterEnd: true }), {
      name: 'TypeError',
      code: 'ERR_TRAILING_JUNK_AFTER_STREAM_END',
    });
  // Gzip treats bytes after a member as the start of another member.
  if (decompress !== 'gunzip' && decompress !== 'unzip') {
    assert.deepStrictEqual(await callAsync(decompress, withJunk), data);
  }
}

(async () => {
  for (const pair of pairs) {
    await testRoundTrip(pair);
    await testErrors(pair);
    await testInfo(pair);
    await testTrailingGarbage(pair);
  }

  // Invalid compressed data.
  for (const [method, expected] of [
    ['inflate', { code: 'Z_DATA_ERROR', errno: Z_DATA_ERROR,
                  message: 'incorrect header check' }],
    ['gunzip', { code: 'Z_DATA_ERROR', errno: Z_DATA_ERROR,
                 message: 'incorrect header check' }],
    ['unzip', { code: 'Z_DATA_ERROR', errno: Z_DATA_ERROR,
                message: 'incorrect header check' }],
    ['inflateRaw', { code: 'Z_DATA_ERROR', errno: Z_DATA_ERROR }],
    ['brotliDecompress', { code: /^ERR__ERROR_FORMAT_/,
                           message: 'Decompression failed' }],
    ['zstdDecompress', { code: 'ZSTD_error_prefix_unknown',
                         message: 'Unknown frame descriptor' }],
  ]) {
    await assert.rejects(
      callAsync(method, Buffer.from('this is not compressed data')),
      { name: 'Error', ...expected });
  }

  // Concatenated gzip members are all decompressed.
  {
    const members = Buffer.concat([zlib.gzipSync('abc'), zlib.gzipSync('def')]);
    assert.strictEqual((await callAsync('gunzip', members)).toString(),
                       'abcdef');
  }

  // Dictionaries.
  {
    const dictionary = Buffer.from('a dictionary for the zlib tests');
    const input = 'zlib tests, a dictionary for the zlib tests';
    const compressed = await callAsync('deflate', input, { dictionary });
    assert.strictEqual(
      (await callAsync('inflate', compressed, { dictionary })).toString(),
      input);
    await assert.rejects(callAsync('inflate', compressed), {
      code: 'Z_NEED_DICT',
      errno: Z_NEED_DICT,
      message: 'Missing dictionary',
    });
    await assert.rejects(
      callAsync('inflate', compressed, { dictionary: Buffer.from('nope') }), {
        code: 'Z_NEED_DICT',
        message: 'Bad dictionary',
      });
  }

  // Non-default flush and compression options still round-trip.
  for (const options of [
    { flush: Z_SYNC_FLUSH },
    { flush: Z_FULL_FLUSH, level: 1 },
    { level: 9, memLevel: 9, windowBits: 9, strategy: zlib.constants.Z_RLE },
  ]) {
    const compressed = await callAsync('deflate', data, options);
    assert.deepStrictEqual(zlib.inflateSync(compressed, options), data);
  }

  // Strings are encoded with the stream defaultEncoding option.
  {
    const compressed = await callAsync('deflate', 'ff00',
                                       { defaultEncoding: 'hex' });
    assert.deepStrictEqual(zlib.inflateSync(compressed),
                           Buffer.from([0xff, 0x00]));
  }

  // Many concurrent calls.
  {
    const inputs = Array.from({ length: 64 },
                              (_, i) => data.subarray(i * 512, i * 1024 + 7));
    const results = await Promise.all(inputs.map(async (input) => {
      const compressed = await callAsync('gzip', input);
      return callAsync('gunzip', compressed);
    }));
    for (let i = 0; i < inputs.length; i++) {
      assert.deepStrictEqual(results[i], inputs[i]);
    }
  }

  // util.promisify() works.
  assert.deepStrictEqual(
    zlib.inflateSync(await promisify(zlib.deflate)(data)), data);

  // The callback runs in the caller's AsyncLocalStorage context.
  {
    const als = new AsyncLocalStorage();
    const compressed = zlib.deflateSync(data);
    await new Promise((resolve) => {
      als.run('store', () => {
        zlib.inflate(compressed, common.mustSucceed((result) => {
          assert.strictEqual(als.getStore(), 'store');
          assert.deepStrictEqual(result, data);
          resolve();
        }));
      });
    });
  }

  // Invalid arguments throw synchronously without calling the callback.
  assert.throws(() => zlib.deflate(1, common.mustNotCall()), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => zlib.deflate(data), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => zlib.deflate(data, { level: 42 }, common.mustNotCall()), {
    name: 'RangeError',
    code: 'ERR_OUT_OF_RANGE',
  });
})().then(common.mustCall());
