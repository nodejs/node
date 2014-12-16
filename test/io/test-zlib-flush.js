var common = require('../common.js');
var assert = require('assert');
var zlib = require('zlib');
var path = require('path');
var fs = require('fs');

var file = fs.readFileSync(path.resolve(common.fixturesDir, 'person.jpg')),
    chunkSize = 16,
    opts = { level: 0 },
    deflater = zlib.createDeflate(opts);

var chunk = file.slice(0, chunkSize),
    expectedNone = new Buffer([0x78, 0x01]),
    blkhdr = new Buffer([0x00, 0x10, 0x00, 0xef, 0xff]),
    adler32 = new Buffer([0x00, 0x00, 0x00, 0xff, 0xff]),
    expectedFull = Buffer.concat([blkhdr, chunk, adler32]),
    actualNone,
    actualFull;

deflater.write(chunk, function() {
  deflater.flush(zlib.Z_NO_FLUSH, function() {
    actualNone = deflater.read();
    deflater.flush(function() {
      var bufs = [], buf;
      while (buf = deflater.read())
        bufs.push(buf);
      actualFull = Buffer.concat(bufs);
    });
  });
});

process.once('exit', function() {
  assert.deepEqual(actualNone, expectedNone);
  assert.deepEqual(actualFull, expectedFull);
});
