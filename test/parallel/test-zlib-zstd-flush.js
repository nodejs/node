'use strict';
require('../common');
const assert = require('assert');
const zlib = require('zlib');
const fixtures = require('../common/fixtures');

const file = fixtures.readSync('person.jpg');
const chunkSize = 16;
const compress = new zlib.ZstdCompress();

const chunk = file.slice(0, chunkSize);
const expectedFull = Buffer.from('KLUv/QBYgAAA/9j/4AAQSkZJRgABAQEASA==', 'base64');
let actualFull;

compress.write(chunk, function() {
  compress.flush(function() {
    const bufs = [];
    let buf;
    while ((buf = compress.read()) !== null)
      bufs.push(buf);
    actualFull = Buffer.concat(bufs);
  });
});

process.once('exit', function() {
  assert.deepStrictEqual(actualFull.toString('base64'), expectedFull.toString('base64'));
  assert.deepStrictEqual(actualFull, expectedFull);
});
