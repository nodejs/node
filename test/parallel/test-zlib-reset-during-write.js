'use strict';
const common = require('../common');
const assert = require('assert');
const { createBrotliCompress, createDeflate } = require('zlib');

// Tests that calling .reset() while an async write is in progress
// throws an error instead of causing a use-after-free.

for (const factory of [createBrotliCompress, createDeflate]) {
  const stream = factory();
  const input = Buffer.alloc(1024, 0x41);

  stream.write(input, common.mustCall());
  stream.on('error', common.mustNotCall());

  // The write has been dispatched to the thread pool.
  // Calling reset while write is in progress must throw.
  assert.throws(() => {
    stream._handle.reset();
  }, {
    message: 'Cannot reset zlib stream while a write is in progress',
  });
}
