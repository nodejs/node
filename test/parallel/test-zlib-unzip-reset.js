'use strict';

const common = require('../common');
const assert = require('node:assert');
const zlib = require('node:zlib');

function testReset(prefix, input, expected) {
  const output = [];
  const unzip = zlib.createUnzip()
    .on('error', common.mustNotCall())
    .on('data', (chunk) => output.push(chunk))
    .on('end', common.mustCall(() => {
      assert.strictEqual(Buffer.concat(output).toString(), expected);
    }));

  unzip.write(prefix, common.mustCall(() => {
    unzip.reset();
    unzip.end(input);
  }));
}

// Reset while gzip auto-detection is halfway through.
testReset(
  Buffer.from([0x1f]),
  zlib.gzipSync('hello'),
  'hello',
);

// Reset after auto-detection selected INFLATE. The new input must be detected
// as GUNZIP so that every concatenated gzip member is processed.
testReset(
  zlib.deflateSync('discarded').subarray(0, 2),
  Buffer.concat([
    zlib.gzipSync('abc'),
    zlib.gzipSync('def'),
    zlib.gzipSync('ghi'),
  ]),
  'abcdefghi',
);
