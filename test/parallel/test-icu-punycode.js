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
  const errorRe = {
    ascii: /^Error: Cannot convert name to ASCII$/,
    unicode: /^Error: Cannot convert name to Unicode$/
  };
  const convertFunc = {
    ascii: icu.toASCII,
    unicode: icu.toUnicode
  };

  for (const [i, { url, mode }] of tests.invalid.entries()) {
    assert.throws(() => convertFunc[mode](url), errorRe[mode],
                  `Invalid case ${i + 1}`);
    assert.doesNotThrow(() => convertFunc[mode](url, true),
                        `Invalid case ${i + 1} in lenient mode`);
  }
}
