// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const util = require('internal/util');

const tests = [
  ['', 'utf8'],
  ['utf8', 'utf8'],
  ['utf-8', 'utf8'],
  ['UTF-8', 'utf8'],
  ['UTF8', 'utf8'],
  ['Utf8', 'utf8'],
  ['uTf-8', 'utf8'],
  ['utF-8', 'utf8'],
  ['ucs2', 'utf16le'],
  ['UCS2', 'utf16le'],
  ['utf16le', 'utf16le'],
  ['utf-16le', 'utf16le'],
  ['UTF-16LE', 'utf16le'],
  ['UTF16LE', 'utf16le'],
  ['binary', 'latin1'],
  ['BINARY', 'latin1'],
  ['latin1', 'latin1'],
  ['base64', 'base64'],
  ['BASE64', 'base64'],
  ['hex', 'hex'],
  ['HEX', 'hex'],
  ['foo', undefined],
  [1, undefined],
  [false, 'utf8'],
  [undefined, 'utf8'],
  [[], undefined],
];

tests.forEach((i) => {
  assert.strictEqual(util.normalizeEncoding(i[0]), i[1]);
});
