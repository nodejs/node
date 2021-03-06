'use strict';

const common = require('../common');
if (!common.hasIntl)
  common.skip('missing Intl');

const strictEqual = require('assert').strictEqual;
const url = require('url');

const domainToASCII = url.domainToASCII;
const domainToUnicode = url.domainToUnicode;

const domainWithASCII = [
  ['ıíd', 'xn--d-iga7r'],
  ['يٴ', 'xn--mhb8f'],
  ['www.ϧƽəʐ.com', 'www.xn--cja62apfr6c.com'],
  ['новини.com', 'xn--b1amarcd.com'],
  ['名がドメイン.com', 'xn--v8jxj3d1dzdz08w.com'],
  ['افغانستا.icom.museum', 'xn--mgbaal8b0b9b2b.icom.museum'],
  ['الجزائر.icom.fake', 'xn--lgbbat1ad8j.icom.fake'],
  ['भारत.org', 'xn--h2brj9c.org']
];

domainWithASCII.forEach((pair) => {
  const domain = pair[0];
  const ascii = pair[1];
  const domainConvertedToASCII = domainToASCII(domain);
  strictEqual(domainConvertedToASCII, ascii);
  const asciiConvertedToUnicode = domainToUnicode(ascii);
  strictEqual(asciiConvertedToUnicode, domain);
});
