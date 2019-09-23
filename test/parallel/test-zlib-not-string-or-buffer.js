'use strict';

// Check the error condition testing for passing something other than a string
// or buffer.

const common = require('../common');
const zlib = require('zlib');

[
  undefined,
  null,
  true,
  false,
  0,
  1,
  [1, 2, 3],
  { foo: 'bar' }
].forEach((input) => {
  common.expectsError(
    () => zlib.deflateSync(input),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "buffer" argument must be of type string or an instance ' +
               'of Buffer, TypedArray, DataView, or ArrayBuffer.' +
               common.invalidArgTypeHelper(input)
    }
  );
});
