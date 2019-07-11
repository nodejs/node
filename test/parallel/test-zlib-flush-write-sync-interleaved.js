'use strict';
const common = require('../common');
const assert = require('assert');
const { createGzip, createGunzip, Z_PARTIAL_FLUSH } = require('zlib');

// Verify that .flush() behaves like .write() in terms of ordering, e.g. in
// a sequence like .write() + .flush() + .write() + .flush() each .flush() call
// only affects the data written before it.
// Refs: https://github.com/nodejs/node/issues/28478

const compress = createGzip();
const decompress = createGunzip();
decompress.setEncoding('utf8');

const events = [];
const compressedChunks = [];

for (const chunk of ['abc', 'def', 'ghi']) {
  compress.write(chunk, common.mustCall(() => events.push({ written: chunk })));
  compress.flush(Z_PARTIAL_FLUSH, common.mustCall(() => {
    events.push('flushed');
    const chunk = compress.read();
    if (chunk !== null)
      compressedChunks.push(chunk);
  }));
}

compress.end(common.mustCall(() => {
  events.push('compress end');
  writeToDecompress();
}));

function writeToDecompress() {
  // Write the compressed chunks to a decompressor, one by one, in order to
  // verify that the flushes actually worked.
  const chunk = compressedChunks.shift();
  if (chunk === undefined) return decompress.end();
  decompress.write(chunk, common.mustCall(() => {
    events.push({ read: decompress.read() });
    writeToDecompress();
  }));
}

process.on('exit', () => {
  assert.deepStrictEqual(events, [
    { written: 'abc' },
    'flushed',
    { written: 'def' },
    'flushed',
    { written: 'ghi' },
    'flushed',
    'compress end',
    { read: 'abc' },
    { read: 'def' },
    { read: 'ghi' }
  ]);
});
