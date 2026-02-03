'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const zlib = require('zlib');

// Test some zstd-specific properties of the zstd streams that can not
// be easily covered through expanding zlib-only tests.

const sampleBuffer = fixtures.readSync('/pss-vectors.json');

{
  // Test setting the quality parameter at stream creation:
  const sizes = [];
  for (let quality = 1;
    quality <= 22;
    quality++) {
    const encoded = zlib.zstdCompressSync(sampleBuffer, {
      params: {
        [zlib.constants.ZSTD_c_compressionLevel]: quality
      }
    });
    sizes.push(encoded.length);
  }

  // Increasing quality should roughly correspond to decreasing compressed size:
  for (let i = 0; i < sizes.length - 1; i++) {
    assert(sizes[i + 1] <= sizes[i] * 1.05, sizes);  // 5 % margin of error.
  }
  assert(sizes[0] > sizes[sizes.length - 1], sizes);
}

{
  // Test that setting out-of-bounds option values or keys fails.
  assert.throws(() => {
    zlib.createZstdCompress({
      params: {
        10000: 0
      }
    });
  }, {
    code: 'ERR_ZSTD_INVALID_PARAM',
    name: 'RangeError',
    message: '10000 is not a valid zstd parameter'
  });

  // Test that accidentally using duplicate keys fails.
  assert.throws(() => {
    zlib.createZstdCompress({
      params: {
        '0': 0,
        '00': 0
      }
    });
  }, {
    code: 'ERR_ZSTD_INVALID_PARAM',
    name: 'RangeError',
    message: '00 is not a valid zstd parameter'
  });

  assert.throws(() => {
    zlib.createZstdCompress({
      params: {
        // This param must be a valid ZSTD_strategy value.
        [zlib.constants.ZSTD_c_strategy]: 130
      }
    });
  }, {
    code: 'ERR_ZLIB_INITIALIZATION_FAILED',
    name: 'Error',
    message: 'Setting parameter failed'
  });

  // Test that setting out-of-bounds option values or keys fails.
  assert.throws(() => {
    zlib.createZstdDecompress({
      params: {
        10000: 0
      }
    });
  }, {
    code: 'ERR_ZSTD_INVALID_PARAM',
    name: 'RangeError',
    message: '10000 is not a valid zstd parameter'
  });

  // Test that accidentally using duplicate keys fails.
  assert.throws(() => {
    zlib.createZstdDecompress({
      params: {
        '0': 0,
        '00': 0
      }
    });
  }, {
    code: 'ERR_ZSTD_INVALID_PARAM',
    name: 'RangeError',
    message: '00 is not a valid zstd parameter'
  });

  assert.throws(() => {
    zlib.createZstdDecompress({
      params: {
        // This param must be >= 10 (ZSTD_WINDOWLOG_ABSOLUTEMIN).
        [zlib.constants.ZSTD_d_windowLogMax]: 1
      }
    });
  }, {
    code: 'ERR_ZLIB_INITIALIZATION_FAILED',
    name: 'Error',
    message: 'Setting parameter failed'
  });
}

{
  // Test options.flush range
  assert.throws(() => {
    zlib.zstdCompressSync('', { flush: zlib.constants.Z_FINISH });
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.flush" is out of range. It must be >= 0 ' +
      'and <= 2. Received 4',
  });

  assert.throws(() => {
    zlib.zstdCompressSync('', { finishFlush: zlib.constants.Z_FINISH });
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.finishFlush" is out of range. It must be ' +
      '>= 0 and <= 2. Received 4',
  });
}
