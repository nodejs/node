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
common.expectsError(
  () => new zlib.Deflate({ chunkSize: -Infinity }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

// Confirm that maximum chunk size cannot be exceeded because it is `Infinity`.
assert.strictEqual(zlib.constants.Z_MAX_CHUNK, Infinity);

// Throws if `opts.windowBits` is invalid
common.expectsError(
  () => new zlib.Deflate({ windowBits: -Infinity }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

common.expectsError(
  () => new zlib.Deflate({ windowBits: Infinity }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

// Throws if `opts.level` is invalid
common.expectsError(
  () => new zlib.Deflate({ level: -Infinity }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

common.expectsError(
  () => new zlib.Deflate({ level: Infinity }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

// Throws a RangeError if `level` invalid in  `Deflate.prototype.params()`
common.expectsError(
  () => new zlib.Deflate().params(-Infinity),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    type: RangeError
  }
);

common.expectsError(
  () => new zlib.Deflate().params(Infinity),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    type: RangeError
  }
);

// Throws if `opts.memLevel` is invalid
common.expectsError(
  () => new zlib.Deflate({ memLevel: -Infinity }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
);

common.expectsError(
  () => new zlib.Deflate({ memLevel: Infinity }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: RangeError
  }
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
common.expectsError(
  () => new zlib.Deflate({ strategy: String(zlib.constants.Z_RLE) }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: TypeError
  }
);

// Throws if opts.strategy is invalid
common.expectsError(
  () => new zlib.Deflate({ strategy: 'this is a bogus strategy' }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: TypeError
  }
);

// Throws TypeError if `strategy` is invalid in `Deflate.prototype.params()`
common.expectsError(
  () => new zlib.Deflate().params(0, 'I am an invalid strategy'),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    type: TypeError
  }
);

// Throws if opts.dictionary is not a Buffer
common.expectsError(
  () => new zlib.Deflate({ dictionary: 'not a buffer' }),
  {
    code: 'ERR_INVALID_OPT_VALUE',
    type: TypeError
  }
);
