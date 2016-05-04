'use strict';
// https://github.com/nodejs/node/issues/6034

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const decompress = zlib.createGunzip(15);

decompress.on('error', common.mustCall((err) => {
  assert.strictEqual(decompress._closed, true);
  assert.doesNotThrow(() => decompress.close());
}));

assert.strictEqual(decompress._closed, false);
decompress.write('something invalid');
