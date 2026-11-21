'use strict';

// Refs: https://github.com/nodejs/node/issues/64619

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

function resetAfter(prefix, input, expected) {
  const chunks = [];
  const unzip = zlib.createUnzip();
  unzip.on('error', common.mustNotCall());
  unzip.on('data', (chunk) => chunks.push(chunk));
  unzip.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(chunks).toString(), expected);
  }));
  unzip.write(prefix, common.mustCall(() => {
    unzip.reset();
    unzip.end(input);
  }));
}

// Reset in the middle of the gzip magic number.
resetAfter(Buffer.from([0x1f]), zlib.gzipSync('hello'), 'hello');

// Reset after deflate was detected.
resetAfter(
  zlib.deflateSync('discarded').subarray(0, 2),
  Buffer.concat([zlib.gzipSync('abc'), zlib.gzipSync('def'), zlib.gzipSync('ghi')]),
  'abcdefghi',
);

// Reset after gzip was detected.
resetAfter(
  zlib.gzipSync('discarded').subarray(0, 4),
  zlib.deflateSync('hello'),
  'hello',
);
