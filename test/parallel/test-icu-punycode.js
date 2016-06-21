'use strict';

const common = require('../common');
const icu = getPunycode();
const assert = require('assert');

function getPunycode() {
  try {
    return process.binding('icu');
  } catch (err) {
    return undefined;
  }
}

if (!icu) {
  common.skip('icu punycode tests because ICU is not present.');
  return;
}

// Credit for list: http://www.i18nguy.com/markup/idna-examples.html
const tests = [
  'افغانستا.icom.museum',
  'الجزائر.icom.museum',
  'österreich.icom.museum',
  'বাংলাদেশ.icom.museum',
  'беларусь.icom.museum',
  'belgië.icom.museum',
  'българия.icom.museum',
  'تشادر.icom.museum',
  '中国.icom.museum',
  'القمر.icom.museum',
  'κυπρος.icom.museum',
  'českárepublika.icom.museum',
  'مصر.icom.museum',
  'ελλάδα.icom.museum',
  'magyarország.icom.museum',
  'ísland.icom.museum',
  'भारत.icom.museum',
  'ايران.icom.museum',
  'éire.icom.museum',
  'איקו״ם.ישראל.museum',
  '日本.icom.museum',
  'الأردن.icom.museum',
  'қазақстан.icom.museum',
  '한국.icom.museum',
  'кыргызстан.icom.museum',
  'ລາວ.icom.museum',
  'لبنان.icom.museum',
  'македонија.icom.museum',
  'موريتانيا.icom.museum',
  'méxico.icom.museum',
  'монголулс.icom.museum',
  'المغرب.icom.museum',
  'नेपाल.icom.museum',
  'عمان.icom.museum',
  'قطر.icom.museum',
  'românia.icom.museum',
  'россия.иком.museum',
  'србијаицрнагора.иком.museum',
  'இலங்கை.icom.museum',
  'españa.icom.museum',
  'ไทย.icom.museum',
  'تونس.icom.museum',
  'türkiye.icom.museum',
  'украина.icom.museum',
  'việtnam.icom.museum'
];

// Testing the roundtrip
tests.forEach((i) => {
  assert.strictEqual(i, icu.toUnicode(icu.toASCII(i)));
});
