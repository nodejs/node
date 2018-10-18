'use strict';

require('../common');

const assert = require('assert');
const zlib = require('zlib');

// verify that the zlib transform does clean up
// the handle when calling destroy.

const ts = zlib.createGzip();
ts.destroy();
assert.strictEqual(ts._handle, null);
