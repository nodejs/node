// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const util = require('internal/util');

const tests = [
  [undefined, 'utf8'],
  [null, 'utf8'],
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
  ['UcS2', 'utf16le'],
  ['ucs-2', 'utf16le'],
  ['UCS-2', 'utf16le'],
  ['UcS-2', 'utf16le'],
  ['utf16le', 'utf16le'],
  ['utf-16le', 'utf16le'],
  ['UTF-16LE', 'utf16le'],
  ['UTF16LE', 'utf16le'],
  ['binary', 'latin1'],
  ['BINARY', 'latin1'],
  ['latin1', 'latin1'],
  ['LaTiN1', 'latin1'],
  ['base64', 'base64'],
  ['BASE64', 'base64'],
  ['Base64', 'base64'],
  ['hex', 'hex'],
  ['HEX', 'hex'],
  ['ASCII', 'ascii'],
  ['AsCii', 'ascii'],
  ['foo', undefined],
  [1, undefined],
  [false, undefined],
  [NaN, undefined],
  [0, undefined],
  [[], undefined],
  [{}, undefined]
];

tests.forEach((e, i) => {
  const res = util.normalizeEncoding(e[0]);
  assert.strictEqual(res, e[1], `#${i} failed: expected ${e[1]}, got ${res}`);
});
