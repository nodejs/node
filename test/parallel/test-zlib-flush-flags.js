'use strict';
require('../common');
const assert = require('assert');
const zlib = require('zlib');

assert.doesNotThrow(() => {
  zlib.createGzip({ flush: zlib.constants.Z_SYNC_FLUSH });
});

assert.throws(() => {
  zlib.createGzip({ flush: 'foobar' });
}, /^RangeError: Invalid flush flag: foobar$/);

assert.throws(() => {
  zlib.createGzip({ flush: 10000 });
}, /^RangeError: Invalid flush flag: 10000$/);

assert.doesNotThrow(() => {
  zlib.createGzip({ finishFlush: zlib.constants.Z_SYNC_FLUSH });
});

assert.throws(() => {
  zlib.createGzip({ finishFlush: 'foobar' });
}, /^RangeError: Invalid flush flag: foobar$/);

assert.throws(() => {
  zlib.createGzip({ finishFlush: 10000 });
}, /^RangeError: Invalid flush flag: 10000$/);
