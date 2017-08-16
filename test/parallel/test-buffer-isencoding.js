'use strict';

require('../common');
const assert = require('assert');

[
  'hex',
  'utf8',
  'utf-8',
  'ascii',
  'latin1',
  'binary',
  'base64',
  'ucs2',
  'ucs-2',
  'utf16le',
  'utf-16le'
].forEach((enc) => {
  assert.strictEqual(Buffer.isEncoding(enc), true);
});

[
  'utf9',
  'utf-7',
  'Unicode-FTW',
  'new gnu gun',
  false,
  NaN,
  {},
  Infinity,
  [],
  1,
  0,
  -1
].forEach((enc) => {
  assert.strictEqual(Buffer.isEncoding(enc), false);
});
