'use strict';
require('../common');
const assert = require('assert');
const { ZipArchive } = require('zlib');

const fooFile = Buffer.from('Hello'.repeat(100));

const zip = new ZipArchive();
zip.addFile('foo.txt', fooFile);
const buf = zip.digest();

const zip2 = new ZipArchive(buf);
const toc = zip2.getEntries();

assert.deepStrictEqual(toc, new Map([
  [ 'foo.txt', 0 ],
]));

assert.deepStrictEqual(zip2.readEntry(0), fooFile);
