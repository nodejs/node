'use strict';

require('../common');

const assert = require('assert');
const zlib = require('zlib');

assert.throws(
  () => zlib.createGzip({ chunkSize: 0 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  }
);

assert.throws(
  () => zlib.createGzip({ windowBits: 0 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  }
);

assert.throws(
  () => zlib.createGzip({ memLevel: 0 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  }
);

{
  const stream = zlib.createGzip({ level: NaN });
  assert.strictEqual(stream._level, zlib.constants.Z_DEFAULT_COMPRESSION);
}

{
  const stream = zlib.createGzip({ strategy: NaN });
  assert.strictEqual(stream._strategy, zlib.constants.Z_DEFAULT_STRATEGY);
}
