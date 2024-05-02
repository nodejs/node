'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const zlib = require('zlib');

// Test some brotli-specific properties of the brotli streams that can not
// be easily covered through expanding zlib-only tests.

const sampleBuffer = fixtures.readSync('/pss-vectors.json');

{
  // Test setting the quality parameter at stream creation:
  const sizes = [];
  for (let quality = zlib.constants.BROTLI_MIN_QUALITY;
    quality <= zlib.constants.BROTLI_MAX_QUALITY;
    quality++) {
    const encoded = zlib.brotliCompressSync(sampleBuffer, {
      params: {
        [zlib.constants.BROTLI_PARAM_QUALITY]: quality
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
    zlib.createBrotliCompress({
      params: {
        10000: 0
      }
    });
  }, {
    code: 'ERR_BROTLI_INVALID_PARAM',
    name: 'RangeError',
    message: '10000 is not a valid Brotli parameter'
  });

  // Test that accidentally using duplicate keys fails.
  assert.throws(() => {
    zlib.createBrotliCompress({
      params: {
        '0': 0,
        '00': 0
      }
    });
  }, {
    code: 'ERR_BROTLI_INVALID_PARAM',
    name: 'RangeError',
    message: '00 is not a valid Brotli parameter'
  });

  assert.throws(() => {
    zlib.createBrotliCompress({
      params: {
        // This is a boolean flag
        [zlib.constants.BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING]: 42
      }
    });
  }, {
    code: 'ERR_ZLIB_INITIALIZATION_FAILED',
    name: 'Error',
    message: 'Initialization failed'
  });
}

{
  // Test options.flush range
  assert.throws(() => {
    zlib.brotliCompressSync('', { flush: zlib.constants.Z_FINISH });
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.flush" is out of range. It must be >= 0 ' +
      'and <= 3. Received 4',
  });

  assert.throws(() => {
    zlib.brotliCompressSync('', { finishFlush: zlib.constants.Z_FINISH });
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.finishFlush" is out of range. It must be ' +
      '>= 0 and <= 3. Received 4',
  });
}
