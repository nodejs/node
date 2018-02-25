'use strict';
const common = require('../common');
const zlib = require('zlib');

zlib.createGzip({ flush: zlib.constants.Z_SYNC_FLUSH });

common.expectsError(
  () => zlib.createGzip({ flush: 'foobar' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options.flush" property must be of type number. ' +
             'Received type string'
  }
);

common.expectsError(
  () => zlib.createGzip({ flush: 10000 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.flush" is out of range. It must ' +
             'be >= 0 and <= 5. Received 10000'
  }
);

zlib.createGzip({ finishFlush: zlib.constants.Z_SYNC_FLUSH });

common.expectsError(
  () => zlib.createGzip({ finishFlush: 'foobar' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options.finishFlush" property must be of type number. ' +
             'Received type string'
  }
);

common.expectsError(
  () => zlib.createGzip({ finishFlush: 10000 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.finishFlush" is out of range. It must ' +
             'be >= 0 and <= 5. Received 10000'
  }
);
