'use strict';

const common = require('../common');

const zlib = require('zlib');
const assert = require('assert');

// Work with and without `new` keyword
assert.ok(zlib.Deflate() instanceof zlib.Deflate);
assert.ok(new zlib.Deflate() instanceof zlib.Deflate);

assert.ok(zlib.DeflateRaw() instanceof zlib.DeflateRaw);
assert.ok(new zlib.DeflateRaw() instanceof zlib.DeflateRaw);

// Throws if `opts.chunkSize` is invalid
assert.throws(
  () => { new zlib.Deflate({ chunkSize: -Infinity }); },
  common.expectsError({
    code: 'ERR_LESS_THAN_MIN',
    type: RangeError,
    message: '"chunkSize" is out of range. It should be equal or greater ' +
    `than ${zlib.constants.Z_MIN_CHUNK}.`
  })
);

// Confirm that maximum chunk size cannot be exceeded because it is `Infinity`.
assert.strictEqual(zlib.constants.Z_MAX_CHUNK, Infinity);

// Throws if `opts.windowBits` is invalid
assert.throws(
  () => { new zlib.Deflate({ windowBits: -Infinity }); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"windowBits" is out of range. It should be between ' +
    `${zlib.constants.Z_MIN_WINDOWBITS} and ${zlib.constants.Z_MAX_WINDOWBITS}.`
  })
);

assert.throws(
  () => { new zlib.Deflate({ windowBits: Infinity }); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"windowBits" is out of range. It should be between ' +
    `${zlib.constants.Z_MIN_WINDOWBITS} and ${zlib.constants.Z_MAX_WINDOWBITS}.`
  })
);

// Throws if `opts.level` is invalid
assert.throws(
  () => { new zlib.Deflate({ level: -Infinity }); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"level" is out of range. It should be between ' +
    `${zlib.constants.Z_MIN_LEVEL} and ${zlib.constants.Z_MAX_LEVEL}.`
  })
);

assert.throws(
  () => { new zlib.Deflate({ level: Infinity }); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"level" is out of range. It should be between ' +
    `${zlib.constants.Z_MIN_LEVEL} and ${zlib.constants.Z_MAX_LEVEL}.`
  })
);

// Throws a RangeError if `level` invalid in  `Deflate.prototype.params()`
assert.throws(
  () => { new zlib.Deflate().params(-Infinity); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"level" is out of range. It should be between ' +
    `${zlib.constants.Z_MIN_LEVEL} and ${zlib.constants.Z_MAX_LEVEL}.`
  })
);

assert.throws(
  () => { new zlib.Deflate().params(Infinity); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"level" is out of range. It should be between ' +
    `${zlib.constants.Z_MIN_LEVEL} and ${zlib.constants.Z_MAX_LEVEL}.`
  })
);

// Throws if `opts.memLevel` is invalid
assert.throws(
  () => { new zlib.Deflate({ memLevel: -Infinity }); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"memLevel" is out of range. It should be between ' +
    `${zlib.constants.Z_MIN_MEMLEVEL} and ${zlib.constants.Z_MAX_MEMLEVEL}.`
  })
);

assert.throws(
  () => { new zlib.Deflate({ memLevel: Infinity }); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"memLevel" is out of range. It should be between ' +
    `${zlib.constants.Z_MIN_MEMLEVEL} and ${zlib.constants.Z_MAX_MEMLEVEL}.`
  })
);

// Does not throw if opts.strategy is valid
assert.doesNotThrow(
  () => { new zlib.Deflate({ strategy: zlib.constants.Z_FILTERED }); }
);

assert.doesNotThrow(
  () => { new zlib.Deflate({ strategy: zlib.constants.Z_HUFFMAN_ONLY }); }
);

assert.doesNotThrow(
  () => { new zlib.Deflate({ strategy: zlib.constants.Z_RLE }); }
);

assert.doesNotThrow(
  () => { new zlib.Deflate({ strategy: zlib.constants.Z_FIXED }); }
);

assert.doesNotThrow(
  () => { new zlib.Deflate({ strategy: zlib.constants.Z_DEFAULT_STRATEGY }); }
);

// Throws if opt.strategy is the wrong type.
assert.throws(
  () => { new zlib.Deflate({ strategy: String(zlib.constants.Z_RLE) }); },
  common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "strategy" argument must be of type number'
  })
);

// Throws if opts.strategy is invalid
assert.throws(
  () => { new zlib.Deflate({ strategy: 'this is a bogus strategy' }); },
  common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "strategy" argument must be of type number'
  })
);

assert.throws(
  () => { new zlib.Deflate({ strategy: -Infinity }); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"strategy" is out of range. It should be between ' +
    `${zlib.constants.Z_DEFAULT_STRATEGY} and ${zlib.constants.Z_FIXED}.`
  })
);

assert.throws(
  () => { new zlib.Deflate({ strategy: Infinity }); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"strategy" is out of range. It should be between ' +
    `${zlib.constants.Z_DEFAULT_STRATEGY} and ${zlib.constants.Z_FIXED}.`
  })
);

// Throws TypeError if `strategy` is invalid in `Deflate.prototype.params()`
assert.throws(
  () => { new zlib.Deflate().params(0, 'I am an invalid strategy'); },
  common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "strategy" argument must be of type number'
  })
);

assert.throws(
  () => { new zlib.Deflate().params(0, -Infinity); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"strategy" is out of range. It should be between ' +
    `${zlib.constants.Z_DEFAULT_STRATEGY} and ${zlib.constants.Z_FIXED}.`
  })
);

assert.throws(
  () => { new zlib.Deflate().params(0, Infinity); },
  common.expectsError({
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: '"strategy" is out of range. It should be between ' +
    `${zlib.constants.Z_DEFAULT_STRATEGY} and ${zlib.constants.Z_FIXED}.`
  })
);

// Throws if opts.dictionary is not a Buffer
assert.throws(
  () => { new zlib.Deflate({ dictionary: 'not a buffer' }); },
  common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "dictionary" argument must be one of type Buffer, ' +
    'TypedArray or DataView'
  })
);
