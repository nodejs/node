'use strict';
require('../common');
const assert = require('assert');
const zlib = require('zlib');
const fixtures = require('../common/fixtures');

const file = fixtures.readSync('person.jpg');
const chunkSize = 16;
const opts = { level: 0 };
const deflater = zlib.createDeflate(opts);

const chunk = file.slice(0, chunkSize);
const expectedNone = Buffer.from([0x78, 0x01]);
const blkhdr = Buffer.from([0x00, 0x10, 0x00, 0xef, 0xff]);
const adler32 = Buffer.from([0x00, 0x00, 0x00, 0xff, 0xff]);
const expectedFull = Buffer.concat([blkhdr, chunk, adler32]);
let actualNone;
let actualFull;

deflater.write(chunk, function() {
  deflater.flush(zlib.constants.Z_NO_FLUSH, function() {
    actualNone = deflater.read();
    deflater.flush(function() {
      const bufs = [];
      let buf;
      while ((buf = deflater.read()) !== null)
        bufs.push(buf);
      actualFull = Buffer.concat(bufs);
    });
  });
});

process.once('exit', function() {
  assert.deepStrictEqual(actualNone, expectedNone);
  assert.deepStrictEqual(actualFull, expectedFull);
});
