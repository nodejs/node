'use strict';

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const dictionary = Buffer.from(
  `Lorem ipsum dolor sit amet, consectetur adipiscing elit. 
  Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. 
  Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.`
);

const input = Buffer.from(
  `Lorem ipsum dolor sit amet, consectetur adipiscing elit. 
  Lorem ipsum dolor sit amet, consectetur adipiscing elit. 
  Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. 
  Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. 
  Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.`
);

zlib.zstdCompress(input, { dictionary }, common.mustSucceed((compressed) => {
  assert(compressed.length < input.length);
  zlib.zstdDecompress(compressed, { dictionary }, common.mustSucceed((decompressed) => {
    assert.strictEqual(decompressed.toString(), input.toString());
  }));
}));
