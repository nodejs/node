'use strict';
require('../common');
const assert = require('assert');
const { ZipArchive, crc32Sync, deflateRawSync } = require('zlib');

const fooFile = Buffer.from('Hello'.repeat(100));
const compressedFooFile = deflateRawSync(fooFile);
const crc = crc32Sync(fooFile);

const zip = new ZipArchive();
zip.addFile('foo.txt', compressedFooFile, { compress: false, size: fooFile.length, crc });
const buf = zip.digest();

const zip2 = new ZipArchive(buf);
const toc = zip2.getEntries();

assert.deepStrictEqual(toc, new Map([
  [ 'foo.txt', 0 ],
]));

assert.deepStrictEqual(zip2.readEntry(0), fooFile);
