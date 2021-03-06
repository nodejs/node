'use strict';
const common = require('../common');
const zlib = require('zlib');
const assert = require('assert');

const input = '0123456789'.repeat(4);

for (const [ compress, decompressor ] of [
  [ zlib.deflateRawSync, zlib.createInflateRaw ],
  [ zlib.deflateSync, zlib.createInflate ],
  [ zlib.brotliCompressSync, zlib.createBrotliDecompress ]
]) {
  const compressed = compress(input);
  const trailingData = Buffer.from('not valid compressed data');

  for (const variant of [
    (stream) => { stream.end(compressed); },
    (stream) => { stream.write(compressed); stream.write(trailingData); },
    (stream) => { stream.write(compressed); stream.end(trailingData); },
    (stream) => { stream.write(Buffer.concat([compressed, trailingData])); },
    (stream) => { stream.end(Buffer.concat([compressed, trailingData])); }
  ]) {
    let output = '';
    const stream = decompressor();
    stream.setEncoding('utf8');
    stream.on('data', (chunk) => output += chunk);
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(output, input);
      assert.strictEqual(stream.bytesWritten, compressed.length);
    }));
    variant(stream);
  }
}
