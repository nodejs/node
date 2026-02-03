'use strict';

const { hasIntl } = require('../common');

const assert = require('node:assert');
const { domainToASCII, domainToUnicode } = require('node:url');
const { test } = require('node:test');

const domainWithASCII = [
  ['ıíd', 'xn--d-iga7r'],
  ['يٴ', 'xn--mhb8f'],
  ['www.ϧƽəʐ.com', 'www.xn--cja62apfr6c.com'],
  ['новини.com', 'xn--b1amarcd.com'],
  ['名がドメイン.com', 'xn--v8jxj3d1dzdz08w.com'],
  ['افغانستا.icom.museum', 'xn--mgbaal8b0b9b2b.icom.museum'],
  ['الجزائر.icom.fake', 'xn--lgbbat1ad8j.icom.fake'],
  ['भारत.org', 'xn--h2brj9c.org'],
];

test('domainToASCII and domainToUnicode', { skip: !hasIntl }, () => {
  for (const [domain, ascii] of domainWithASCII) {
    const domainConvertedToASCII = domainToASCII(domain);
    assert.strictEqual(domainConvertedToASCII, ascii);
    const asciiConvertedToUnicode = domainToUnicode(ascii);
    assert.strictEqual(asciiConvertedToUnicode, domain);
  }
});
