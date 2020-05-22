'use strict';
require('../common');
const assert = require('assert');
const zlib = require('zlib');

const encoded = Buffer.from('G38A+CXCIrFAIAM=', 'base64');

// Async
zlib.brotliDecompress(encoded, { maxOutputLength: 64 }, function(err) {
  assert.ok(err instanceof RangeError);
});

// Sync
assert.throws(function() {
  zlib.brotliDecompressSync(encoded, { maxOutputLength: 64 });
}, RangeError);

// Async
zlib.brotliDecompress(encoded, { maxOutputLength: 256 }, function(err) {
  assert.strictEqual(err, null);
});

// Sync
zlib.brotliDecompressSync(encoded, { maxOutputLength: 256 });
