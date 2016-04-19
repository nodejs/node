'use strict';
var common = require('../common');
var assert = require('assert');
var zlib = require('zlib');
var path = require('path');
var fs = require('fs');

const file = fs.readFileSync(path.resolve(common.fixturesDir, 'person.jpg'));
const chunkSize = 12 * 1024;
const opts = { level: 9, strategy: zlib.Z_DEFAULT_STRATEGY };
const deflater = zlib.createDeflate(opts);

const chunk1 = file.slice(0, chunkSize);
const chunk2 = file.slice(chunkSize);
const blkhdr = Buffer.from([0x00, 0x5a, 0x82, 0xa5, 0x7d]);
const expected = Buffer.concat([blkhdr, chunk2]);
let actual;

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
  assert.deepStrictEqual(actual, expected);
});
