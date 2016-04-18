'use strict';
// https://github.com/nodejs/node/issues/6034

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const decompress = zlib.createGunzip(15);

decompress.on('error', common.mustCall((err) => {
  assert.doesNotThrow(() => decompress.close());
}));

decompress.write('something invalid');
