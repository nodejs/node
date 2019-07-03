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

for (const chunk of ['abc', 'def', 'ghi']) {
  compress.write(chunk, common.mustCall(() => events.push({ written: chunk })));
  compress.flush(Z_PARTIAL_FLUSH, common.mustCall(() => {
    events.push('flushed');
    decompress.write(compress.read(), common.mustCall(() => {
      events.push({ read: decompress.read() });
    }));
  }));
}

process.on('exit', () => {
  assert.deepStrictEqual(events, [
    { written: 'abc' },
    'flushed',
    { written: 'def' },
    { read: 'abc' },
    'flushed',
    { written: 'ghi' },
    { read: 'def' },
    'flushed',
    { read: 'ghi' }
  ]);
});
