'use strict';
require('../common');
const assert = require('assert');
const {
  createBrotliCompress,
  createBrotliDecompress,
  createDeflate,
  createZstdCompress,
  createZstdDecompress,
} = require('zlib');

// Tests that calling init() on the internal handle while an async write is in
// progress throws an error instead of releasing state still used by the
// thread pool.

const factories = [createBrotliCompress, createBrotliDecompress, createDeflate];
if (typeof createZstdCompress === 'function') {
  factories.push(createZstdCompress, createZstdDecompress);
}

for (const factory of factories) {
  const stream = factory();
  const input = Buffer.alloc(1024, 0x41);

  stream.write(input, () => {});
  // The decompressors are fed invalid data and are expected to error later.
  stream.on('error', () => {});
  stream.on('data', () => {});

  // The write has been dispatched to the thread pool.
  assert.throws(() => {
    stream._handle.init();
  }, {
    message: 'Cannot initialize zlib stream while a write is in progress',
  });
}
