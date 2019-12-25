'use strict';

require('../common');

const zlib = require('zlib');
const assert = require('assert');

// Work with and without `new` keyword
assert.ok(zlib.Deflate() instanceof zlib.Deflate);
assert.ok(new zlib.Deflate() instanceof zlib.Deflate);

assert.ok(zlib.DeflateRaw() instanceof zlib.DeflateRaw);
assert.ok(new zlib.DeflateRaw() instanceof zlib.DeflateRaw);

// Throws if `options.chunkSize` is invalid
assert.throws(
  () => new zlib.Deflate({ chunkSize: 'test' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.chunkSize" property must be of type number. ' +
             "Received type string ('test')"
  }
);

assert.throws(
  () => new zlib.Deflate({ chunkSize: -Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.chunkSize" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate({ chunkSize: 0 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.chunkSize" is out of range. It must ' +
             'be >= 64. Received 0'
  }
);

// Confirm that maximum chunk size cannot be exceeded because it is `Infinity`.
assert.strictEqual(zlib.constants.Z_MAX_CHUNK, Infinity);

// Throws if `options.windowBits` is invalid
assert.throws(
  () => new zlib.Deflate({ windowBits: 'test' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.windowBits" property must be of type number. ' +
             "Received type string ('test')"
  }
);

assert.throws(
  () => new zlib.Deflate({ windowBits: -Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.windowBits" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate({ windowBits: Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.windowBits" is out of range. It must ' +
             'be a finite number. Received Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate({ windowBits: 0 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.windowBits" is out of range. It must ' +
             'be >= 8 and <= 15. Received 0'
  }
);

// Throws if `options.level` is invalid
assert.throws(
  () => new zlib.Deflate({ level: 'test' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.level" property must be of type number. ' +
             "Received type string ('test')"
  }
);

assert.throws(
  () => new zlib.Deflate({ level: -Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.level" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate({ level: Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.level" is out of range. It must ' +
             'be a finite number. Received Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate({ level: -2 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.level" is out of range. It must ' +
             'be >= -1 and <= 9. Received -2'
  }
);

// Throws if `level` invalid in  `Deflate.prototype.params()`
assert.throws(
  () => new zlib.Deflate().params('test'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "level" argument must be of type number. ' +
             "Received type string ('test')"
  }
);

assert.throws(
  () => new zlib.Deflate().params(-Infinity),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "level" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate().params(Infinity),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "level" is out of range. It must ' +
             'be a finite number. Received Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate().params(-2),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "level" is out of range. It must ' +
             'be >= -1 and <= 9. Received -2'
  }
);

// Throws if options.memLevel is invalid
assert.throws(
  () => new zlib.Deflate({ memLevel: 'test' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.memLevel" property must be of type number. ' +
             "Received type string ('test')"
  }
);

assert.throws(
  () => new zlib.Deflate({ memLevel: -Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.memLevel" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate({ memLevel: Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.memLevel" is out of range. It must ' +
             'be a finite number. Received Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate({ memLevel: -2 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.memLevel" is out of range. It must ' +
             'be >= 1 and <= 9. Received -2'
  }
);

// Does not throw if opts.strategy is valid
new zlib.Deflate({ strategy: zlib.constants.Z_FILTERED });
new zlib.Deflate({ strategy: zlib.constants.Z_HUFFMAN_ONLY });
new zlib.Deflate({ strategy: zlib.constants.Z_RLE });
new zlib.Deflate({ strategy: zlib.constants.Z_FIXED });
new zlib.Deflate({ strategy: zlib.constants.Z_DEFAULT_STRATEGY });

// Throws if options.strategy is invalid
assert.throws(
  () => new zlib.Deflate({ strategy: 'test' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.strategy" property must be of type number. ' +
             "Received type string ('test')"
  }
);

assert.throws(
  () => new zlib.Deflate({ strategy: -Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.strategy" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate({ strategy: Infinity }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.strategy" is out of range. It must ' +
             'be a finite number. Received Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate({ strategy: -2 }),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "options.strategy" is out of range. It must ' +
             'be >= 0 and <= 4. Received -2'
  }
);

// Throws TypeError if `strategy` is invalid in `Deflate.prototype.params()`
assert.throws(
  () => new zlib.Deflate().params(0, 'test'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "strategy" argument must be of type number. ' +
             "Received type string ('test')"
  }
);

assert.throws(
  () => new zlib.Deflate().params(0, -Infinity),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "strategy" is out of range. It must ' +
             'be a finite number. Received -Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate().params(0, Infinity),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "strategy" is out of range. It must ' +
             'be a finite number. Received Infinity'
  }
);

assert.throws(
  () => new zlib.Deflate().params(0, -2),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "strategy" is out of range. It must ' +
             'be >= 0 and <= 4. Received -2'
  }
);

// Throws if opts.dictionary is not a Buffer
assert.throws(
  () => new zlib.Deflate({ dictionary: 'not a buffer' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.dictionary" property must be an instance of Buffer' +
             ', TypedArray, DataView, or ArrayBuffer. Received type string ' +
             "('not a buffer')"
  }
);
