'use strict';

// Check the error condition testing for passing something other than a string
// or buffer.

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const expected = common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "buffer" argument must be one of type string, Buffer, ' +
  'TypedArray or DataView'
});

assert.throws(() => { zlib.deflateSync(undefined); }, expected);
assert.throws(() => { zlib.deflateSync(null); }, expected);
assert.throws(() => { zlib.deflateSync(true); }, expected);
assert.throws(() => { zlib.deflateSync(false); }, expected);
assert.throws(() => { zlib.deflateSync(0); }, expected);
assert.throws(() => { zlib.deflateSync(1); }, expected);
assert.throws(() => { zlib.deflateSync([1, 2, 3]); }, expected);
assert.throws(() => { zlib.deflateSync({foo: 'bar'}); }, expected);
