// Flags: --expose-brotli
'use strict';

const common = require('../common');

const brotli = require('brotli');
const assert = require('assert');

// Throws if `options.chunkSize` is invalid
common.expectsError(
  () => new brotli.Compress({ chunkSize: 'test' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options.chunkSize" property must be of type number. ' +
             'Received type string'
  }
);

common.expectsError(
  () => new brotli.Compress({ chunkSize: -Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.chunkSize" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

common.expectsError(
  () => new brotli.Compress({ chunkSize: 0 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.chunkSize" is out of range. It must ' +
             'be >= 64 and <= Infinity. Received 0'
  }
);

// Confirm that maximum chunk size cannot be exceeded because it is `Infinity`.
assert.strictEqual(brotli.constants.BROTLI_MAX_CHUNK, Infinity);

// Throws if `options.lgwin` is invalid
common.expectsError(
  () => new brotli.Compress({ lgwin: 'test' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options.lgwin" property must be of type number. ' +
             'Received type string'
  }
);

common.expectsError(
  () => new brotli.Compress({ lgwin: -Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.lgwin" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

common.expectsError(
  () => new brotli.Compress({ lgwin: Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.lgwin" is out of range. It must ' +
             'be a finite number. Received Infinity'
  }
);

common.expectsError(
  () => new brotli.Compress({ lgwin: 0 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.lgwin" is out of range. It must ' +
             'be >= 10 and <= 24. Received 0'
  }
);

// Throws if `options.quality` is invalid
common.expectsError(
  () => new brotli.Compress({ quality: 'test' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options.quality" property must be of type number. ' +
             'Received type string'
  }
);

common.expectsError(
  () => new brotli.Compress({ quality: -Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.quality" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

common.expectsError(
  () => new brotli.Compress({ quality: Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.quality" is out of range. It must ' +
             'be a finite number. Received Infinity'
  }
);

common.expectsError(
  () => new brotli.Compress({ quality: -2 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.quality" is out of range. It must ' +
             'be >= 0 and <= 11. Received -2'
  }
);

// Does not throw if opts.mode is valid
new brotli.Compress({ mode: brotli.constants.BROTLI_MODE_GENERIC });
new brotli.Compress({ mode: brotli.constants.BROTLI_MODE_GENERIC });
new brotli.Compress({ mode: brotli.constants.BROTLI_MODE_TEXT });
new brotli.Compress({ mode: brotli.constants.BROTLI_MODE_FONT });

// Throws if options.mode is invalid
common.expectsError(
  () => new brotli.Compress({ mode: 'test' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options.mode" property must be of type number. ' +
             'Received type string'
  }
);

common.expectsError(
  () => new brotli.Compress({ mode: -Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.mode" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

common.expectsError(
  () => new brotli.Compress({ mode: Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.mode" is out of range. It must ' +
             'be a finite number. Received Infinity'
  }
);

common.expectsError(
  () => new brotli.Compress({ mode: -2 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: 'The value of "options.mode" is out of range. It must ' +
             'be >= 0 and <= 2. Received -2'
  }
);
