'use strict';

require('../common');

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
  /^RangeError: Invalid chunk size: -Infinity$/
);

// Confirm that maximum chunk size cannot be exceeded because it is `Infinity`.
assert.strictEqual(zlib.constants.Z_MAX_CHUNK, Infinity);

// Throws if `opts.windowBits` is invalid
assert.throws(
  () => { new zlib.Deflate({ windowBits: -Infinity }); },
  /^RangeError: Invalid windowBits: -Infinity$/
);

assert.throws(
  () => { new zlib.Deflate({ windowBits: Infinity }); },
  /^RangeError: Invalid windowBits: Infinity$/
);

// Throws if `opts.level` is invalid
assert.throws(
  () => { new zlib.Deflate({ level: -Infinity }); },
  /^RangeError: Invalid compression level: -Infinity$/
);

assert.throws(
  () => { new zlib.Deflate({ level: Infinity }); },
  /^RangeError: Invalid compression level: Infinity$/
);

// Throws a RangeError if `level` invalid in  `Deflate.prototype.params()`
assert.throws(
  () => { new zlib.Deflate().params(-Infinity); },
  /^RangeError: Invalid compression level: -Infinity$/
);

assert.throws(
  () => { new zlib.Deflate().params(Infinity); },
  /^RangeError: Invalid compression level: Infinity$/
);

// Throws if `opts.memLevel` is invalid
assert.throws(
  () => { new zlib.Deflate({ memLevel: -Infinity }); },
  /^RangeError: Invalid memLevel: -Infinity$/
);

assert.throws(
  () => { new zlib.Deflate({ memLevel: Infinity }); },
  /^RangeError: Invalid memLevel: Infinity$/
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
  /^TypeError: Invalid strategy: 3$/
);

// Throws if opts.strategy is invalid
assert.throws(
  () => { new zlib.Deflate({ strategy: 'this is a bogus strategy' }); },
  /^TypeError: Invalid strategy: this is a bogus strategy$/
);

// Throws TypeError if `strategy` is invalid in `Deflate.prototype.params()`
assert.throws(
  () => { new zlib.Deflate().params(0, 'I am an invalid strategy'); },
  /^TypeError: Invalid strategy: I am an invalid strategy$/
);

// Throws if opts.dictionary is not a Buffer
assert.throws(
  () => { new zlib.Deflate({ dictionary: 'not a buffer' }); },
  /^TypeError: Invalid dictionary: it should be a Buffer, TypedArray, or DataView$/
);
