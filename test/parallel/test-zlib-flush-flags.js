'use strict';
const common = require('../common');
const zlib = require('zlib');

zlib.createGzip({ flush: zlib.constants.Z_SYNC_FLUSH });

common.expectsError(
  () => zlib.createGzip({ flush: 'foobar' }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

common.expectsError(
  () => zlib.createGzip({ flush: 10000 }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

zlib.createGzip({ finishFlush: zlib.constants.Z_SYNC_FLUSH });

common.expectsError(
  () => zlib.createGzip({ finishFlush: 'foobar' }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

common.expectsError(
  () => zlib.createGzip({ finishFlush: 10000 }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);
