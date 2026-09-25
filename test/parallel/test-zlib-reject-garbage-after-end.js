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

async function collect(stream, ...inputs) {
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));
  for (let i = 0; i < inputs.length - 1; i++) {
    stream.write(inputs[i]);
  }
  stream.end(inputs[inputs.length - 1]);
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
    trailingInput: Buffer.from('trailing junk'),
  },
];

for (const {
  label,
  compress,
  decompress,
  decompressSync,
  createDecompress,
  defaultOutput,
  trailingInput,
} of cases) {
  test(`rejectGarbageAfterEnd rejects trailing input for ${label}`, async () => {
    const compressed = compress(Buffer.from('a'));
    const withTrailingInput = Buffer.concat([
      compressed,
      trailingInput ?? compressed,
    ]);

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

test('zstd decompresses concatenated frames regardless of chunking', async () => {
  const first = zlib.zstdCompressSync('a');
  const second = zlib.zstdCompressSync('b');
  const skippable = Buffer.alloc(12);
  skippable.writeUInt32LE(0x184d2a50, 0);
  skippable.writeUInt32LE(4, 4);
  skippable.write('meta', 8);

  for (const input of [
    Buffer.concat([first, second]),
    Buffer.concat([first, skippable, second]),
  ]) {
    for (const rejectGarbageAfterEnd of [false, true]) {
      const options = { rejectGarbageAfterEnd };
      assert.strictEqual(
        zlib.zstdDecompressSync(input, options).toString(),
        'ab',
      );
      assert.strictEqual(
        (await callAsync(zlib.zstdDecompress, input, options)).toString(),
        'ab',
      );

      for (let split = 0; split <= input.length; split++) {
        assert.strictEqual(
          (await collect(
            zlib.createZstdDecompress(options),
            input.subarray(0, split),
            input.subarray(split),
          )).toString(),
          'ab',
          `split at byte ${split}`,
        );
      }
    }
  }
});

test('zstd trailing junk handling is independent of chunking', async () => {
  const compressed = zlib.zstdCompressSync('a');
  const laterFrame = zlib.zstdCompressSync('b');
  const junk = Buffer.from('trailing junk');

  assert.strictEqual(
    (await collect(
      zlib.createZstdDecompress(),
      compressed,
      junk,
      laterFrame,
    )).toString(),
    'a',
  );
  await assert.rejects(
    collect(
      zlib.createZstdDecompress({ rejectGarbageAfterEnd: true }),
      compressed,
      junk,
    ),
    trailingJunkError,
  );
});

test('zstd handles incomplete trailing frame identifiers as junk', async () => {
  const compressed = zlib.zstdCompressSync('a');
  const framePrefix = zlib.zstdCompressSync('b').subarray(0, 3);

  for (let length = 1; length <= framePrefix.length; length++) {
    const trailing = framePrefix.subarray(0, length);
    assert.strictEqual(
      zlib.zstdDecompressSync(Buffer.concat([compressed, trailing])).toString(),
      'a',
    );
    assert.throws(
      () => zlib.zstdDecompressSync(Buffer.concat([compressed, trailing]), {
        rejectGarbageAfterEnd: true,
      }),
      trailingJunkError,
    );
    assert.strictEqual(
      (await collect(
        zlib.createZstdDecompress(),
        compressed,
        trailing,
      )).toString(),
      'a',
    );
  }
});

test('zstd reports errors in subsequent frames', async () => {
  const first = zlib.zstdCompressSync('a');
  const second = zlib.zstdCompressSync('b', {
    params: {
      [zlib.constants.ZSTD_c_checksumFlag]: 1,
    },
  });
  second[second.length - 1] ^= 1;
  const input = Buffer.concat([first, second]);
  const checksumError = { code: 'ZSTD_error_checksum_wrong' };

  assert.throws(() => zlib.zstdDecompressSync(input), checksumError);
  await assert.rejects(
    collect(zlib.createZstdDecompress(), first, second),
    checksumError,
  );
});

test('zstd decompresses multiple frames across output buffers', async () => {
  const firstInput = Buffer.allocUnsafe(1024);
  const secondInput = Buffer.allocUnsafe(1024);
  for (let i = 0; i < firstInput.length; i++) {
    firstInput[i] = i;
    secondInput[i] = i + 1;
  }
  const input = Buffer.concat([
    zlib.zstdCompressSync(firstInput),
    zlib.zstdCompressSync(secondInput),
  ]);
  const expected = Buffer.concat([firstInput, secondInput]);
  const options = { chunkSize: 64 };

  assert.deepStrictEqual(zlib.zstdDecompressSync(input, options), expected);
  assert.deepStrictEqual(
    await collect(zlib.createZstdDecompress(options), input),
    expected,
  );
});

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
