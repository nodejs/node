'use strict';
require('../common');
const assert = require('assert');
const { ZipArchive } = require('zlib');

const emptyArchive = new Uint8Array([
  0x50, 0x4B, 0x05, 0x06,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
]);

const zip = new ZipArchive();

const toc = zip.getEntries();
assert.deepStrictEqual(toc, new Map());

const buf = zip.digest();
assert.deepStrictEqual(buf.buffer, emptyArchive.buffer);
