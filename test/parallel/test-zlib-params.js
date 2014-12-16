var common = require('../common.js');
var assert = require('assert');
var zlib = require('zlib');
var path = require('path');
var fs = require('fs');

var file = fs.readFileSync(path.resolve(common.fixturesDir, 'person.jpg')),
    chunkSize = 24 * 1024,
    opts = { level: 9, strategy: zlib.Z_DEFAULT_STRATEGY },
    deflater = zlib.createDeflate(opts);

var chunk1 = file.slice(0, chunkSize),
    chunk2 = file.slice(chunkSize),
    blkhdr = new Buffer([0x00, 0x48, 0x82, 0xb7, 0x7d]),
    expected = Buffer.concat([blkhdr, chunk2]),
    actual;

deflater.write(chunk1, function() {
  deflater.params(0, zlib.Z_DEFAULT_STRATEGY, function() {
    while (deflater.read());
    deflater.end(chunk2, function() {
      var bufs = [], buf;
      while (buf = deflater.read())
        bufs.push(buf);
      actual = Buffer.concat(bufs);
    });
  });
  while (deflater.read());
});

process.once('exit', function() {
  assert.deepEqual(actual, expected);
});
