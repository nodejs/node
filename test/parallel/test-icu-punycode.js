'use strict';
const common = require('../common');

if (!common.hasIntl) {
  common.skip('missing Intl');
  return;
}

const icu = process.binding('icu');
const assert = require('assert');

const tests = require('../fixtures/url-idna.js');

{
  for (const [i, { ascii, unicode }] of tests.valid.entries()) {
    assert.strictEqual(ascii, icu.toASCII(unicode), `toASCII(${i + 1})`);
    assert.strictEqual(unicode, icu.toUnicode(ascii), `toUnicode(${i + 1})`);
    assert.strictEqual(ascii, icu.toASCII(icu.toUnicode(ascii)),
                       `toASCII(toUnicode(${i + 1}))`);
    assert.strictEqual(unicode, icu.toUnicode(icu.toASCII(unicode)),
                       `toUnicode(toASCII(${i + 1}))`);
  }
}

{
  for (const [i, url] of tests.invalid.entries()) {
    assert.throws(() => icu.toASCII(url),
                  /^Error: Cannot convert name to ASCII$/,
                  `ToASCII invalid case ${i + 1}`);
    assert.doesNotThrow(() => icu.toASCII(url, true),
                        `ToASCII invalid case ${i + 1} in lenient mode`);
    assert.doesNotThrow(() => icu.toUnicode(url),
                        `ToUnicode invalid case ${i + 1}`);
  }
}
