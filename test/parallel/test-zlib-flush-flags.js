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
    message: 'The "options.flush" property must be of type number. ' +
             "Received type string ('foobar')"
  }
);

assert.throws(
  () => zlib.createGzip({ flush: 10000 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.flush" is out of range. It must ' +
             'be >= 0 and <= 5. Received 10000'
  }
);

zlib.createGzip({ finishFlush: zlib.constants.Z_SYNC_FLUSH });

assert.throws(
  () => zlib.createGzip({ finishFlush: 'foobar' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.finishFlush" property must be of type number. ' +
             "Received type string ('foobar')"
  }
);

assert.throws(
  () => zlib.createGzip({ finishFlush: 10000 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.finishFlush" is out of range. It must ' +
             'be >= 0 and <= 5. Received 10000'
  }
);
