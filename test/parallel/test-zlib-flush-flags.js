'use strict';
require('../common');
const assert = require('assert');
const zlib = require('zlib');

zlib.createGzip({ flush: zlib.constants.Z_SYNC_FLUSH });

assert.throws(
  () => zlib.createGzip({ flush: 'foobar' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

assert.throws(
  () => zlib.createGzip({ flush: 10000 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  }
);

zlib.createGzip({ finishFlush: zlib.constants.Z_SYNC_FLUSH });

assert.throws(
  () => zlib.createGzip({ finishFlush: 'foobar' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

assert.throws(
  () => zlib.createGzip({ finishFlush: 10000 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  }
);
