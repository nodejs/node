'use strict';

// Check the error condition testing for passing something other than a string
// or buffer.

require('../common');
const assert = require('assert');
const zlib = require('zlib');

// Passing nothing (undefined) instead of string/buffer to `zlib.deflateSync()`
assert.throws(zlib.deflateSync, /^TypeError: Not a string or buffer$/);
