// Flags: --expose-brotli
'use strict';
const common = require('../common');
const assert = require('assert');
const brotli = require('brotli');

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

let compWriter;
let compData = Buffer.alloc(0);

const comp = new brotli.Compress();
comp.on('data', function(d) {
  compData = Buffer.concat([compData, d]);
  assert.strictEqual(this.bytesWritten, compWriter.size,
                     'Should get write size on Compress data.');
});
comp.on('end', common.mustCall(function() {
  assert.strictEqual(this.bytesWritten, compWriter.size,
                     'Should get write size on Compress end.');
  assert.strictEqual(this.bytesWritten, expectStr.length,
                     'Should get data size on Compress end.');

  {
    let decompWriter;
    let decompData = Buffer.alloc(0);

    const decomp = new brotli.Decompress();
    decomp.on('data', function(d) {
      decompData = Buffer.concat([decompData, d]);
      assert.strictEqual(this.bytesWritten, decompWriter.size,
                         'Should get write size on Compress/' +
                          'Decompress data.');
    });
    decomp.on('end', common.mustCall(function() {
      assert.strictEqual(this.bytesWritten, compData.length,
                         'Should get compressed size on Compress/' +
                          'Decompress end.');
      assert.strictEqual(decompData.toString(), expectStr,
                         'Should get original string on Compress/' +
                          'Decompress end.');
    }));
    decompWriter = createWriter(decomp, compData);
  }

  // Some methods should allow extra data after the compressed data
  {
    const compDataExtra = Buffer.concat([compData, Buffer.from('extra')]);

    let decompWriter;
    let decompData = Buffer.alloc(0);

    const decomp = new brotli.Decompress();
    decomp.on('data', function(d) {
      decompData = Buffer.concat([decompData, d]);
      assert.strictEqual(this.bytesWritten, decompWriter.size,
                         'Should get write size on Compress/' +
                          'Decompress data.');
    });
    decomp.on('end', common.mustCall(function() {
      assert.strictEqual(this.bytesWritten, compData.length,
                         'Should get compressed size on Compress/' +
                          'Decompress end.');
      assert.strictEqual(decompData.toString(), expectStr,
                         'Should get original string on Compress/' +
                          'Decompress end.');
    }));
    decompWriter = createWriter(decomp, compDataExtra);
  }
}));
compWriter = createWriter(comp, expectBuf);
