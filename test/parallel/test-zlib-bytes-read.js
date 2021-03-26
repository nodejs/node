'use strict';
const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const expectStr = 'abcdefghijklmnopqrstuvwxyz'.repeat(2);
const expectBuf = Buffer.from(expectStr);

function createWriter(target, buffer) {
  const writer = { size: 0 };
  const write = () => {
    target.write(Buffer.from([buffer[writer.size++]]), () => {
      if (writer.size < buffer.length) {
        target.flush(write);
      } else {
        target.end();
      }
    });
  };
  write();
  return writer;
}

common.expectWarning(
  'DeprecationWarning',
  'zlib.bytesRead is deprecated and will change its meaning in the ' +
  'future. Use zlib.bytesWritten instead.',
  'DEP0108');

for (const method of [
  ['createGzip', 'createGunzip', false],
  ['createGzip', 'createUnzip', false],
  ['createDeflate', 'createInflate', true],
  ['createDeflateRaw', 'createInflateRaw', true],
  ['createBrotliCompress', 'createBrotliDecompress', true],
]) {
  let compWriter;
  let compData = Buffer.alloc(0);

  const comp = zlib[method[0]]();
  comp.on('data', function(d) {
    compData = Buffer.concat([compData, d]);
    assert.strictEqual(this.bytesWritten, compWriter.size,
                       `Should get write size on ${method[0]} data.`);
  });
  comp.on('end', common.mustCall(function() {
    assert.strictEqual(this.bytesWritten, compWriter.size,
                       `Should get write size on ${method[0]} end.`);
    assert.strictEqual(this.bytesWritten, expectStr.length,
                       `Should get data size on ${method[0]} end.`);

    {
      let decompWriter;
      let decompData = Buffer.alloc(0);

      const decomp = zlib[method[1]]();
      decomp.on('data', function(d) {
        decompData = Buffer.concat([decompData, d]);
        assert.strictEqual(this.bytesWritten, decompWriter.size,
                           `Should get write size on ${method[0]}/` +
                           `${method[1]} data.`);
      });
      decomp.on('end', common.mustCall(function() {
        assert.strictEqual(this.bytesWritten, compData.length,
                           `Should get compressed size on ${method[0]}/` +
                           `${method[1]} end.`);
        assert.strictEqual(decompData.toString(), expectStr,
                           `Should get original string on ${method[0]}/` +
                           `${method[1]} end.`);
      }));
      decompWriter = createWriter(decomp, compData);
    }

    // Some methods should allow extra data after the compressed data
    if (method[2]) {
      const compDataExtra = Buffer.concat([compData, Buffer.from('extra')]);

      let decompWriter;
      let decompData = Buffer.alloc(0);

      const decomp = zlib[method[1]]();
      decomp.on('data', function(d) {
        decompData = Buffer.concat([decompData, d]);
        assert.strictEqual(this.bytesWritten, decompWriter.size,
                           `Should get write size on ${method[0]}/` +
                           `${method[1]} data.`);
      });
      decomp.on('end', common.mustCall(function() {
        assert.strictEqual(this.bytesWritten, compData.length,
                           `Should get compressed size on ${method[0]}/` +
                           `${method[1]} end.`);
        // Checking legacy name.
        assert.strictEqual(this.bytesWritten, this.bytesRead);
        assert.strictEqual(decompData.toString(), expectStr,
                           `Should get original string on ${method[0]}/` +
                           `${method[1]} end.`);
      }));
      decompWriter = createWriter(decomp, compDataExtra);
    }
  }));
  compWriter = createWriter(comp, expectBuf);
}
