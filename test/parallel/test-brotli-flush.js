// Flags: --expose-brotli
'use strict';
require('../common');
const assert = require('assert');
const brotli = require('brotli');
const fixtures = require('../common/fixtures');

const file = fixtures.readSync('person.jpg');
const chunkSize = 16;
const deflater = new brotli.Compress();

const chunk = file.slice(0, chunkSize);
const expectedFull = Buffer.from('iweA/9j/4AAQSkZJRgABAQEASA==', 'base64');
let actualFull;

deflater.write(chunk, function() {
  deflater.flush(function() {
    const bufs = [];
    let buf;
    while (buf = deflater.read())
      bufs.push(buf);
    actualFull = Buffer.concat(bufs);
  });
});

process.once('exit', function() {
  assert.deepStrictEqual(actualFull, expectedFull);
});
