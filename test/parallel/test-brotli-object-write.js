// Flags: --expose-brotli
'use strict';

require('../common');
const assert = require('assert');
const { Decompress } = require('brotli');

const decompress = new Decompress({ objectMode: true });
assert.throws(
  () => decompress.write({}),
  TypeError
);
