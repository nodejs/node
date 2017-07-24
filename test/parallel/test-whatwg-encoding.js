// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { TextEncoder, TextDecoder } = require('util');
const { customInspectSymbol: inspect } = require('internal/util');
const { getEncodingFromLabel } = require('internal/encoding');

const encoded = Buffer.from([0xef, 0xbb, 0xbf, 0x74, 0x65,
                             0x73, 0x74, 0xe2, 0x82, 0xac]);

if (!common.hasIntl) {
  common.skip('WHATWG Encoding tests because ICU is not present.');
}

// Make Sure TextDecoder and TextEncoder exist
assert(TextDecoder);
assert(TextEncoder);

// Test TextEncoder
const enc = new TextEncoder();
assert(enc);
const buf = enc.encode('\ufefftest€');

assert.strictEqual(Buffer.compare(buf, encoded), 0);


// Test TextDecoder, UTF-8, fatal: false, ignoreBOM: false
{
  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i);
    const res = dec.decode(buf);
    assert.strictEqual(res, 'test€');
  });

  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i);
    let res = '';
    res += dec.decode(buf.slice(0, 8), { stream: true });
    res += dec.decode(buf.slice(8));
    assert.strictEqual(res, 'test€');
  });
}

// Test TextDecoder, UTF-8, fatal: false, ignoreBOM: true
{
  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i, { ignoreBOM: true });
    const res = dec.decode(buf);
    assert.strictEqual(res, '\ufefftest€');
  });

  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i, { ignoreBOM: true });
    let res = '';
    res += dec.decode(buf.slice(0, 8), { stream: true });
    res += dec.decode(buf.slice(8));
    assert.strictEqual(res, '\ufefftest€');
  });
}

// Test TextDecoder, UTF-8, fatal: true, ignoreBOM: false
{
  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i, { fatal: true });
    assert.throws(() => dec.decode(buf.slice(0, 8)),
                  common.expectsError({
                    code: 'ERR_ENCODING_INVALID_ENCODED_DATA',
                    type: TypeError,
                    message:
                      /^The encoded data was not valid for encoding utf-8$/
                  }));
  });

  ['unicode-1-1-utf-8', 'utf8', 'utf-8'].forEach((i) => {
    const dec = new TextDecoder(i, { fatal: true });
    assert.doesNotThrow(() => dec.decode(buf.slice(0, 8), { stream: true }));
    assert.doesNotThrow(() => dec.decode(buf.slice(8)));
  });
}

// Test TextDecoder, UTF-16le
{
  const dec = new TextDecoder('utf-16le');
  const res = dec.decode(Buffer.from('test€', 'utf-16le'));
  assert.strictEqual(res, 'test€');
}

// Test TextDecoder, UTF-16be
{
  const dec = new TextDecoder('utf-16be');
  const res = dec.decode(Buffer.from([0x00, 0x74, 0x00, 0x65, 0x00,
                                      0x73, 0x00, 0x74, 0x20, 0xac]));
  assert.strictEqual(res, 'test€');
}

{
  const fn = TextDecoder.prototype[inspect];
  fn.call(new TextDecoder(), Infinity, {});

  [{}, [], true, 1, '', new TextEncoder()].forEach((i) => {
    assert.throws(() => fn.call(i, Infinity, {}),
                  common.expectsError({
                    code: 'ERR_INVALID_THIS',
                    message: 'Value of "this" must be of type TextDecoder'
                  }));
  });
}

{
  const fn = TextEncoder.prototype[inspect];
  fn.call(new TextEncoder(), Infinity, {});

  [{}, [], true, 1, '', new TextDecoder()].forEach((i) => {
    assert.throws(() => fn.call(i, Infinity, {}),
                  common.expectsError({
                    code: 'ERR_INVALID_THIS',
                    message: 'Value of "this" must be of type TextEncoder'
                  }));
  });
}

// Test Encoding Mappings
{

  const mappings = {
    'utf-8': [
      'unicode-1-1-utf-8',
      'utf8'
    ],
    'utf-16be': [],
    'utf-16le': [
      'utf-16'
    ],
    'ibm866': [
      '866',
      'cp866',
      'csibm866'
    ],
    'iso-8859-2': [
      'csisolatin2',
      'iso-ir-101',
      'iso8859-2',
      'iso88592',
      'iso_8859-2',
      'iso_8859-2:1987',
      'l2',
      'latin2'
    ],
    'iso-8859-3': [
      'csisolatin3',
      'iso-ir-109',
      'iso8859-3',
      'iso88593',
      'iso_8859-3',
      'iso_8859-3:1988',
      'l3',
      'latin3'
    ],
    'iso-8859-4': [
      'csisolatin4',
      'iso-ir-110',
      'iso8859-4',
      'iso88594',
      'iso_8859-4',
      'iso_8859-4:1988',
      'l4',
      'latin4'
    ],
    'iso-8859-5': [
      'csisolatincyrillic',
      'cyrillic',
      'iso-ir-144',
      'iso8859-5',
      'iso88595',
      'iso_8859-5',
      'iso_8859-5:1988'
    ],
    'iso-8859-6': [
      'arabic',
      'asmo-708',
      'csiso88596e',
      'csiso88596i',
      'csisolatinarabic',
      'ecma-114',
      'iso-8859-6-e',
      'iso-8859-6-i',
      'iso-ir-127',
      'iso8859-6',
      'iso88596',
      'iso_8859-6',
      'iso_8859-6:1987'
    ],
    'iso-8859-7': [
      'csisolatingreek',
      'ecma-118',
      'elot_928',
      'greek',
      'greek8',
      'iso-ir-126',
      'iso8859-7',
      'iso88597',
      'iso_8859-7',
      'iso_8859-7:1987',
      'sun_eu_greek'
    ],
    'iso-8859-8': [
      'csiso88598e',
      'csisolatinhebrew',
      'hebrew',
      'iso-8859-8-e',
      'iso-ir-138',
      'iso8859-8',
      'iso88598',
      'iso_8859-8',
      'iso_8859-8:1988',
      'visual'
    ],
    'iso-8859-8-i': [
      'csiso88598i',
      'logical'
    ],
    'iso-8859-10': [
      'csisolatin6',
      'iso-ir-157',
      'iso8859-10',
      'iso885910',
      'l6',
      'latin6'
    ],
    'iso-8859-13': [
      'iso8859-13',
      'iso885913'
    ],
    'iso-8859-14': [
      'iso8859-14',
      'iso885914'
    ],
    'iso-8859-15': [
      'csisolatin9',
      'iso8859-15',
      'iso885915',
      'iso_8859-15',
      'l9'
    ],
    'koi8-r': [
      'cskoi8r',
      'koi',
      'koi8',
      'koi8_r'
    ],
    'koi8-u': [
      'koi8-ru'
    ],
    'macintosh': [
      'csmacintosh',
      'mac',
      'x-mac-roman'
    ],
    'windows-874': [
      'dos-874',
      'iso-8859-11',
      'iso8859-11',
      'iso885911',
      'tis-620'
    ],
    'windows-1250': [
      'cp1250',
      'x-cp1250'
    ],
    'windows-1251': [
      'cp1251',
      'x-cp1251'
    ],
    'windows-1252': [
      'ansi_x3.4-1968',
      'ascii',
      'cp1252',
      'cp819',
      'csisolatin1',
      'ibm819',
      'iso-8859-1',
      'iso-ir-100',
      'iso8859-1',
      'iso88591',
      'iso_8859-1',
      'iso_8859-1:1987',
      'l1',
      'latin1',
      'us-ascii',
      'x-cp1252'
    ],
    'windows-1253': [
      'cp1253',
      'x-cp1253'
    ],
    'windows-1254': [
      'cp1254',
      'csisolatin5',
      'iso-8859-9',
      'iso-ir-148',
      'iso8859-9',
      'iso88599',
      'iso_8859-9',
      'iso_8859-9:1989',
      'l5',
      'latin5',
      'x-cp1254'
    ],
    'windows-1255': [
      'cp1255',
      'x-cp1255'
    ],
    'windows-1256': [
      'cp1256',
      'x-cp1256'
    ],
    'windows-1257': [
      'cp1257',
      'x-cp1257'
    ],
    'windows-1258': [
      'cp1258',
      'x-cp1258'
    ],
    'x-mac-cyrillic': [
      'x-mac-ukrainian'
    ],
    'gbk': [
      'chinese',
      'csgb2312',
      'csiso58gb231280',
      'gb2312',
      'gb_2312',
      'gb_2312-80',
      'iso-ir-58',
      'x-gbk'
    ],
    'gb18030': [ ],
    'big5': [
      'big5-hkscs',
      'cn-big5',
      'csbig5',
      'x-x-big5'
    ],
    'euc-jp': [
      'cseucpkdfmtjapanese',
      'x-euc-jp'
    ],
    'iso-2022-jp': [
      'csiso2022jp'
    ],
    'shift_jis': [
      'csshiftjis',
      'ms932',
      'ms_kanji',
      'shift-jis',
      'sjis',
      'windows-31j',
      'x-sjis'
    ],
    'euc-kr': [
      '  euc-kr  \t',
      'EUC-kr  \n',
      'cseuckr',
      'csksc56011987',
      'iso-ir-149',
      'korean',
      'ks_c_5601-1987',
      'ks_c_5601-1989',
      'ksc5601',
      'ksc_5601',
      'windows-949'
    ]
  };
  Object.entries(mappings).forEach((i) => {
    const enc = i[0];
    const labels = i[1];
    assert.strictEqual(getEncodingFromLabel(enc), enc);
    labels.forEach((l) => assert.strictEqual(getEncodingFromLabel(l), enc));
  });

  assert.strictEqual(getEncodingFromLabel('made-up'), undefined);
}
