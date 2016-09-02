'use strict';

// Check the error condition testing for passing something other than a string
// or buffer.

require('../common');
const assert = require('assert');
const zlib = require('zlib');

const expected = /^TypeError: Not a string or buffer$/;

assert.throws(() => { zlib.deflateSync(undefined); }, expected);
assert.throws(() => { zlib.deflateSync(null); }, expected);
assert.throws(() => { zlib.deflateSync(true); }, expected);
assert.throws(() => { zlib.deflateSync(false); }, expected);
assert.throws(() => { zlib.deflateSync(0); }, expected);
assert.throws(() => { zlib.deflateSync(1); }, expected);
assert.throws(() => { zlib.deflateSync([1, 2, 3]); }, expected);
assert.throws(() => { zlib.deflateSync({foo: 'bar'}); }, expected);
