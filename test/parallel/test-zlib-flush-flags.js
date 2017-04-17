'use strict';

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

assert.doesNotThrow(() => {
  zlib.createGzip({ flush: zlib.constants.Z_SYNC_FLUSH });
});

assert.throws(() => {
  zlib.createGzip({ flush: 'foobar' });
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "flush" argument must be of type number' }));

assert.throws(() => {
  zlib.createGzip({ flush: 10000 });
}, common.expectsError({
  code: 'ERR_OUT_OF_RANGE',
  type: RangeError,
  message: `flush is out of range` +
    ` [${zlib.constants.Z_NO_FLUSH} to ${zlib.constants.Z_BLOCK}]` }));

assert.doesNotThrow(() => {
  zlib.createGzip({ finishFlush: zlib.constants.Z_SYNC_FLUSH });
});

assert.throws(() => {
  zlib.createGzip({ finishFlush: 'foobar' });
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "finishFlush" argument must be of type number' }));

assert.throws(() => {
  zlib.createGzip({ finishFlush: 10000 });
}, common.expectsError({
  code: 'ERR_OUT_OF_RANGE',
  type: RangeError,
  message: `finishFlush is out of range` +
    ` [${zlib.constants.Z_NO_FLUSH} to ${zlib.constants.Z_BLOCK}]` }));
