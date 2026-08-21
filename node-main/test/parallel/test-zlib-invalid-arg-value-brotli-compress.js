'use strict';

require('../common');

// This test ensures that the BrotliCompress function throws
// ERR_INVALID_ARG_TYPE when the values of the `params` key-value object are
// neither numbers nor booleans.

const assert = require('assert');
const { BrotliCompress, constants } = require('zlib');

const opts = {
  params: {
    [constants.BROTLI_PARAM_MODE]: 'lol'
  }
};

assert.throws(() => BrotliCompress(opts), {
  code: 'ERR_INVALID_ARG_TYPE'
});
