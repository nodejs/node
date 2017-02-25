'use strict';
const common = require('../common');

if (!common.hasIntl) {
  common.skip('missing Intl');
  return;
}

const assert = require('assert');
const { domainToASCII, domainToUnicode } = require('url');

// Tests below are not from WPT.
const tests = require('../fixtures/url-idna.js');

{
  for (const [i, { ascii, unicode }] of tests.valid.entries()) {
    assert.strictEqual(ascii, domainToASCII(unicode),
                       `domainToASCII(${i + 1})`);
    assert.strictEqual(unicode, domainToUnicode(ascii),
                       `domainToUnicode(${i + 1})`);
    assert.strictEqual(ascii, domainToASCII(domainToUnicode(ascii)),
                       `domainToASCII(domainToUnicode(${i + 1}))`);
    assert.strictEqual(unicode, domainToUnicode(domainToASCII(unicode)),
                       `domainToUnicode(domainToASCII(${i + 1}))`);
  }
}

{
  const convertFunc = {
    ascii: domainToASCII,
    unicode: domainToUnicode
  };

  for (const [i, { url, mode }] of tests.invalid.entries())
    assert.strictEqual(convertFunc[mode](url), '', `Invalid case ${i + 1}`);
}
