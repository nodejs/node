// Flags: --expose-brotli
'use strict';
const common = require('../common');
const brotli = require('brotli');
const { inspect } = require('util');
const assert = require('assert');
const emptyBuffer = Buffer.alloc(0);

common.crashOnUnhandledRejection();

(async function() {
  for (const [ compress, decompress, method ] of [
    [ brotli.compress, brotli.decompress, 'async' ],
    [ brotli.compressSync, brotli.decompressSync, 'sync' ],
  ]) {
    const compressed = await compress(emptyBuffer);
    const decompressed = await decompress(compressed);
    assert.deepStrictEqual(
      emptyBuffer, decompressed,
      `Expected ${inspect(compressed)} to match ${inspect(decompressed)} ` +
      `to match for ${method}`);
  }
})();
