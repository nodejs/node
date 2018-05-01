// Flags: --expose-brotli
'use strict';
// https://github.com/nodejs/node/issues/6034

const common = require('../common');
const assert = require('assert');
const brotli = require('brotli');

const decompress = new brotli.Decompress();

decompress.on('error', common.mustCall((err) => {
  assert.strictEqual(decompress._closed, true);
  decompress.close();
}));

assert.strictEqual(decompress._closed, false);
decompress.write('something invalid');
