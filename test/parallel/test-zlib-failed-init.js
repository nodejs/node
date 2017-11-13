'use strict';

const common = require('../common');

const assert = require('assert');
const zlib = require('zlib');

common.expectsError(
  () => zlib.createGzip({ chunkSize: 0 }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

common.expectsError(
  () => zlib.createGzip({ windowBits: 0 }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

common.expectsError(
  () => zlib.createGzip({ memLevel: 0 }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
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
