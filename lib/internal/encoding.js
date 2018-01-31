'use strict';

// An implementation of the WHATWG Encoding Standard
// https://encoding.spec.whatwg.org

const errors = require('internal/errors');
const kHandle = Symbol('handle');
const kFlags = Symbol('flags');
const kEncoding = Symbol('encoding');
const kDecoder = Symbol('decoder');
const kEncoder = Symbol('encoder');

const {
  getConstructorOf,
  customInspectSymbol: inspect
} = require('internal/util');

const { isArrayBufferView } = require('internal/util/types');

const {
  isArrayBuffer
} = process.binding('util');

const {
  encodeUtf8String
} = process.binding('buffer');

var Buffer;
function lazyBuffer() {
  if (Buffer === undefined)
    Buffer = require('buffer').Buffer;
  return Buffer;
}

const CONVERTER_FLAGS_FLUSH = 0x1;
const CONVERTER_FLAGS_FATAL = 0x2;
const CONVERTER_FLAGS_IGNORE_BOM = 0x4;

const empty = new Uint8Array(0);

const encodings = new Map([
  ['unicode-1-1-utf-8', 'utf-8'],
  ['utf8', 'utf-8'],
  ['utf-8', 'utf-8'],
  ['866', 'ibm866'],
  ['cp866', 'ibm866'],
  ['csibm866', 'ibm866'],
  ['ibm866', 'ibm866'],
  ['csisolatin2', 'iso-8859-2'],
  ['iso-8859-2', 'iso-8859-2'],
  ['iso-ir-101', 'iso-8859-2'],
  ['iso8859-2', 'iso-8859-2'],
  ['iso88592', 'iso-8859-2'],
  ['iso_8859-2', 'iso-8859-2'],
  ['iso_8859-2:1987', 'iso-8859-2'],
  ['l2', 'iso-8859-2'],
  ['latin2', 'iso-8859-2'],
  ['csisolatin3', 'iso-8859-3'],
  ['iso-8859-3', 'iso-8859-3'],
  ['iso-ir-109', 'iso-8859-3'],
  ['iso8859-3', 'iso-8859-3'],
  ['iso88593', 'iso-8859-3'],
  ['iso_8859-3', 'iso-8859-3'],
  ['iso_8859-3:1988', 'iso-8859-3'],
  ['l3', 'iso-8859-3'],
  ['latin3', 'iso-8859-3'],
  ['csisolatin4', 'iso-8859-4'],
  ['iso-8859-4', 'iso-8859-4'],
  ['iso-ir-110', 'iso-8859-4'],
  ['iso8859-4', 'iso-8859-4'],
  ['iso88594', 'iso-8859-4'],
  ['iso_8859-4', 'iso-8859-4'],
  ['iso_8859-4:1988', 'iso-8859-4'],
  ['l4', 'iso-8859-4'],
  ['latin4', 'iso-8859-4'],
  ['csisolatincyrillic', 'iso-8859-5'],
  ['cyrillic', 'iso-8859-5'],
  ['iso-8859-5', 'iso-8859-5'],
  ['iso-ir-144', 'iso-8859-5'],
  ['iso8859-5', 'iso-8859-5'],
  ['iso88595', 'iso-8859-5'],
  ['iso_8859-5', 'iso-8859-5'],
  ['iso_8859-5:1988', 'iso-8859-5'],
  ['arabic', 'iso-8859-6'],
  ['asmo-708', 'iso-8859-6'],
  ['csiso88596e', 'iso-8859-6'],
  ['csiso88596i', 'iso-8859-6'],
  ['csisolatinarabic', 'iso-8859-6'],
  ['ecma-114', 'iso-8859-6'],
  ['iso-8859-6', 'iso-8859-6'],
  ['iso-8859-6-e', 'iso-8859-6'],
  ['iso-8859-6-i', 'iso-8859-6'],
  ['iso-ir-127', 'iso-8859-6'],
  ['iso8859-6', 'iso-8859-6'],
  ['iso88596', 'iso-8859-6'],
  ['iso_8859-6', 'iso-8859-6'],
  ['iso_8859-6:1987', 'iso-8859-6'],
  ['csisolatingreek', 'iso-8859-7'],
  ['ecma-118', 'iso-8859-7'],
  ['elot_928', 'iso-8859-7'],
  ['greek', 'iso-8859-7'],
  ['greek8', 'iso-8859-7'],
  ['iso-8859-7', 'iso-8859-7'],
  ['iso-ir-126', 'iso-8859-7'],
  ['iso8859-7', 'iso-8859-7'],
  ['iso88597', 'iso-8859-7'],
  ['iso_8859-7', 'iso-8859-7'],
  ['iso_8859-7:1987', 'iso-8859-7'],
  ['sun_eu_greek', 'iso-8859-7'],
  ['csiso88598e', 'iso-8859-8'],
  ['csisolatinhebrew', 'iso-8859-8'],
  ['hebrew', 'iso-8859-8'],
  ['iso-8859-8', 'iso-8859-8'],
  ['iso-8859-8-e', 'iso-8859-8'],
  ['iso-ir-138', 'iso-8859-8'],
  ['iso8859-8', 'iso-8859-8'],
  ['iso88598', 'iso-8859-8'],
  ['iso_8859-8', 'iso-8859-8'],
  ['iso_8859-8:1988', 'iso-8859-8'],
  ['visual', 'iso-8859-8'],
  ['csiso88598i', 'iso-8859-8-i'],
  ['iso-8859-8-i', 'iso-8859-8-i'],
  ['logical', 'iso-8859-8-i'],
  ['csisolatin6', 'iso-8859-10'],
  ['iso-8859-10', 'iso-8859-10'],
  ['iso-ir-157', 'iso-8859-10'],
  ['iso8859-10', 'iso-8859-10'],
  ['iso885910', 'iso-8859-10'],
  ['l6', 'iso-8859-10'],
  ['latin6', 'iso-8859-10'],
  ['iso-8859-13', 'iso-8859-13'],
  ['iso8859-13', 'iso-8859-13'],
  ['iso885913', 'iso-8859-13'],
  ['iso-8859-14', 'iso-8859-14'],
  ['iso8859-14', 'iso-8859-14'],
  ['iso885914', 'iso-8859-14'],
  ['csisolatin9', 'iso-8859-15'],
  ['iso-8859-15', 'iso-8859-15'],
  ['iso8859-15', 'iso-8859-15'],
  ['iso885915', 'iso-8859-15'],
  ['iso_8859-15', 'iso-8859-15'],
  ['l9', 'iso-8859-15'],
  ['cskoi8r', 'koi8-r'],
  ['koi', 'koi8-r'],
  ['koi8', 'koi8-r'],
  ['koi8-r', 'koi8-r'],
  ['koi8_r', 'koi8-r'],
  ['koi8-ru', 'koi8-u'],
  ['koi8-u', 'koi8-u'],
  ['csmacintosh', 'macintosh'],
  ['mac', 'macintosh'],
  ['macintosh', 'macintosh'],
  ['x-mac-roman', 'macintosh'],
  ['dos-874', 'windows-874'],
  ['iso-8859-11', 'windows-874'],
  ['iso8859-11', 'windows-874'],
  ['iso885911', 'windows-874'],
  ['tis-620', 'windows-874'],
  ['windows-874', 'windows-874'],
  ['cp1250', 'windows-1250'],
  ['windows-1250', 'windows-1250'],
  ['x-cp1250', 'windows-1250'],
  ['cp1251', 'windows-1251'],
  ['windows-1251', 'windows-1251'],
  ['x-cp1251', 'windows-1251'],
  ['ansi_x3.4-1968', 'windows-1252'],
  ['ascii', 'windows-1252'],
  ['cp1252', 'windows-1252'],
  ['cp819', 'windows-1252'],
  ['csisolatin1', 'windows-1252'],
  ['ibm819', 'windows-1252'],
  ['iso-8859-1', 'windows-1252'],
  ['iso-ir-100', 'windows-1252'],
  ['iso8859-1', 'windows-1252'],
  ['iso88591', 'windows-1252'],
  ['iso_8859-1', 'windows-1252'],
  ['iso_8859-1:1987', 'windows-1252'],
  ['l1', 'windows-1252'],
  ['latin1', 'windows-1252'],
  ['us-ascii', 'windows-1252'],
  ['windows-1252', 'windows-1252'],
  ['x-cp1252', 'windows-1252'],
  ['cp1253', 'windows-1253'],
  ['windows-1253', 'windows-1253'],
  ['x-cp1253', 'windows-1253'],
  ['cp1254', 'windows-1254'],
  ['csisolatin5', 'windows-1254'],
  ['iso-8859-9', 'windows-1254'],
  ['iso-ir-148', 'windows-1254'],
  ['iso8859-9', 'windows-1254'],
  ['iso88599', 'windows-1254'],
  ['iso_8859-9', 'windows-1254'],
  ['iso_8859-9:1989', 'windows-1254'],
  ['l5', 'windows-1254'],
  ['latin5', 'windows-1254'],
  ['windows-1254', 'windows-1254'],
  ['x-cp1254', 'windows-1254'],
  ['cp1255', 'windows-1255'],
  ['windows-1255', 'windows-1255'],
  ['x-cp1255', 'windows-1255'],
  ['cp1256', 'windows-1256'],
  ['windows-1256', 'windows-1256'],
  ['x-cp1256', 'windows-1256'],
  ['cp1257', 'windows-1257'],
  ['windows-1257', 'windows-1257'],
  ['x-cp1257', 'windows-1257'],
  ['cp1258', 'windows-1258'],
  ['windows-1258', 'windows-1258'],
  ['x-cp1258', 'windows-1258'],
  ['x-mac-cyrillic', 'x-mac-cyrillic'],
  ['x-mac-ukrainian', 'x-mac-cyrillic'],
  ['chinese', 'gbk'],
  ['csgb2312', 'gbk'],
  ['csiso58gb231280', 'gbk'],
  ['gb2312', 'gbk'],
  ['gb_2312', 'gbk'],
  ['gb_2312-80', 'gbk'],
  ['gbk', 'gbk'],
  ['iso-ir-58', 'gbk'],
  ['x-gbk', 'gbk'],
  ['gb18030', 'gb18030'],
  ['big5', 'big5'],
  ['big5-hkscs', 'big5'],
  ['cn-big5', 'big5'],
  ['csbig5', 'big5'],
  ['x-x-big5', 'big5'],
  ['cseucpkdfmtjapanese', 'euc-jp'],
  ['euc-jp', 'euc-jp'],
  ['x-euc-jp', 'euc-jp'],
  ['csiso2022jp', 'iso-2022-jp'],
  ['iso-2022-jp', 'iso-2022-jp'],
  ['csshiftjis', 'shift_jis'],
  ['ms932', 'shift_jis'],
  ['ms_kanji', 'shift_jis'],
  ['shift-jis', 'shift_jis'],
  ['shift_jis', 'shift_jis'],
  ['sjis', 'shift_jis'],
  ['windows-31j', 'shift_jis'],
  ['x-sjis', 'shift_jis'],
  ['cseuckr', 'euc-kr'],
  ['csksc56011987', 'euc-kr'],
  ['euc-kr', 'euc-kr'],
  ['iso-ir-149', 'euc-kr'],
  ['korean', 'euc-kr'],
  ['ks_c_5601-1987', 'euc-kr'],
  ['ks_c_5601-1989', 'euc-kr'],
  ['ksc5601', 'euc-kr'],
  ['ksc_5601', 'euc-kr'],
  ['windows-949', 'euc-kr'],
  ['utf-16be', 'utf-16be'],
  ['utf-16le', 'utf-16le'],
  ['utf-16', 'utf-16le']
]);

// Unfortunately, String.prototype.trim also removes non-ascii whitespace,
// so we have to do this manually
function trimAsciiWhitespace(label) {
  var s = 0;
  var e = label.length;
  while (s < e && (
    label[s] === '\u0009' ||
    label[s] === '\u000a' ||
    label[s] === '\u000c' ||
    label[s] === '\u000d' ||
    label[s] === '\u0020')) {
    s++;
  }
  while (e > s && (
    label[e - 1] === '\u0009' ||
    label[e - 1] === '\u000a' ||
    label[e - 1] === '\u000c' ||
    label[e - 1] === '\u000d' ||
    label[e - 1] === '\u0020')) {
    e--;
  }
  return label.slice(s, e);
}

function getEncodingFromLabel(label) {
  const enc = encodings.get(label);
  if (enc !== undefined) return enc;
  return encodings.get(trimAsciiWhitespace(label.toLowerCase()));
}

class TextEncoder {
  constructor() {
    this[kEncoder] = true;
  }

  get encoding() {
    if (this == null || this[kEncoder] !== true)
      throw new errors.TypeError('ERR_INVALID_THIS', 'TextEncoder');
    return 'utf-8';
  }

  encode(input = '') {
    if (this == null || this[kEncoder] !== true)
      throw new errors.TypeError('ERR_INVALID_THIS', 'TextEncoder');
    return encodeUtf8String(`${input}`);
  }

  [inspect](depth, opts) {
    if (this == null || this[kEncoder] !== true)
      throw new errors.TypeError('ERR_INVALID_THIS', 'TextEncoder');
    if (typeof depth === 'number' && depth < 0)
      return opts.stylize('[Object]', 'special');
    var ctor = getConstructorOf(this);
    var obj = Object.create({
      constructor: ctor === null ? TextEncoder : ctor
    });
    obj.encoding = this.encoding;
    // Lazy to avoid circular dependency
    return require('util').inspect(obj, opts);
  }
}

Object.defineProperties(
  TextEncoder.prototype, {
    'encode': { enumerable: true },
    'encoding': { enumerable: true },
    [Symbol.toStringTag]: {
      configurable: true,
      value: 'TextEncoder'
    } });

const { hasConverter, TextDecoder } =
  process.binding('config').hasIntl ?
    makeTextDecoderICU() :
    makeTextDecoderJS();

function hasTextDecoder(encoding = 'utf-8') {
  if (typeof encoding !== 'string')
    throw new errors.Error('ERR_INVALID_ARG_TYPE', 'encoding', 'string');
  return hasConverter(getEncodingFromLabel(encoding));
}

function makeTextDecoderICU() {
  const {
    decode: _decode,
    getConverter,
    hasConverter
  } = process.binding('icu');

  class TextDecoder {
    constructor(encoding = 'utf-8', options = {}) {
      encoding = `${encoding}`;
      if (typeof options !== 'object')
        throw new errors.Error('ERR_INVALID_ARG_TYPE', 'options', 'Object');

      const enc = getEncodingFromLabel(encoding);
      if (enc === undefined)
        throw new errors.RangeError('ERR_ENCODING_NOT_SUPPORTED', encoding);

      var flags = 0;
      if (options !== null) {
        flags |= options.fatal ? CONVERTER_FLAGS_FATAL : 0;
        flags |= options.ignoreBOM ? CONVERTER_FLAGS_IGNORE_BOM : 0;
      }

      const handle = getConverter(enc, flags);
      if (handle === undefined)
        throw new errors.Error('ERR_ENCODING_NOT_SUPPORTED', encoding);

      this[kDecoder] = true;
      this[kHandle] = handle;
      this[kFlags] = flags;
      this[kEncoding] = enc;
    }


    decode(input = empty, options = {}) {
      if (this == null || this[kDecoder] !== true)
        throw new errors.TypeError('ERR_INVALID_THIS', 'TextDecoder');
      if (isArrayBuffer(input)) {
        input = lazyBuffer().from(input);
      } else if (!isArrayBufferView(input)) {
        throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'input',
                                   ['ArrayBuffer', 'ArrayBufferView']);
      }
      if (typeof options !== 'object') {
        throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options', 'Object');
      }

      var flags = 0;
      if (options !== null)
        flags |= options.stream ? 0 : CONVERTER_FLAGS_FLUSH;

      const ret = _decode(this[kHandle], input, flags);
      if (typeof ret === 'number') {
        const err = new errors.TypeError('ERR_ENCODING_INVALID_ENCODED_DATA',
                                         this.encoding);
        err.errno = ret;
        throw err;
      }
      return ret.toString('ucs2');
    }
  }

  return { hasConverter, TextDecoder };
}

function makeTextDecoderJS() {
  var StringDecoder;
  function lazyStringDecoder() {
    if (StringDecoder === undefined)
      ({ StringDecoder } = require('string_decoder'));
    return StringDecoder;
  }

  const kBOMSeen = Symbol('BOM seen');

  function hasConverter(encoding) {
    return encoding === 'utf-8' || encoding === 'utf-16le';
  }

  class TextDecoder {
    constructor(encoding = 'utf-8', options = {}) {
      encoding = `${encoding}`;
      if (typeof options !== 'object')
        throw new errors.Error('ERR_INVALID_ARG_TYPE', 'options', 'Object');

      const enc = getEncodingFromLabel(encoding);
      if (enc === undefined || !hasConverter(enc))
        throw new errors.RangeError('ERR_ENCODING_NOT_SUPPORTED', encoding);

      var flags = 0;
      if (options !== null) {
        if (options.fatal) {
          throw new errors.TypeError('ERR_NO_ICU', '"fatal" option');
        }
        flags |= options.ignoreBOM ? CONVERTER_FLAGS_IGNORE_BOM : 0;
      }

      this[kDecoder] = true;
      // StringDecoder will normalize WHATWG encoding to Node.js encoding.
      this[kHandle] = new (lazyStringDecoder())(enc);
      this[kFlags] = flags;
      this[kEncoding] = enc;
      this[kBOMSeen] = false;
    }

    decode(input = empty, options = {}) {
      if (this == null || this[kDecoder] !== true)
        throw new errors.TypeError('ERR_INVALID_THIS', 'TextDecoder');
      if (isArrayBuffer(input)) {
        input = lazyBuffer().from(input);
      } else if (isArrayBufferView(input)) {
        input = lazyBuffer().from(input.buffer, input.byteOffset,
                                  input.byteLength);
      } else {
        throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'input',
                                   ['ArrayBuffer', 'ArrayBufferView']);
      }
      if (typeof options !== 'object') {
        throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options', 'Object');
      }

      if (this[kFlags] & CONVERTER_FLAGS_FLUSH) {
        this[kBOMSeen] = false;
      }

      if (options !== null && options.stream) {
        this[kFlags] &= ~CONVERTER_FLAGS_FLUSH;
      } else {
        this[kFlags] |= CONVERTER_FLAGS_FLUSH;
      }

      if (!this[kBOMSeen] && !(this[kFlags] & CONVERTER_FLAGS_IGNORE_BOM)) {
        if (this[kEncoding] === 'utf-8') {
          if (input.length >= 3 &&
              input[0] === 0xEF && input[1] === 0xBB && input[2] === 0xBF) {
            input = input.slice(3);
          }
        } else if (this[kEncoding] === 'utf-16le') {
          if (input.length >= 2 && input[0] === 0xFF && input[1] === 0xFE) {
            input = input.slice(2);
          }
        }
        this[kBOMSeen] = true;
      }

      if (this[kFlags] & CONVERTER_FLAGS_FLUSH) {
        return this[kHandle].end(input);
      }

      return this[kHandle].write(input);
    }
  }

  return { hasConverter, TextDecoder };
}

// Mix in some shared properties.
{
  Object.defineProperties(
    TextDecoder.prototype,
    Object.getOwnPropertyDescriptors({
      get encoding() {
        if (this == null || this[kDecoder] !== true)
          throw new errors.TypeError('ERR_INVALID_THIS', 'TextDecoder');
        return this[kEncoding];
      },

      get fatal() {
        if (this == null || this[kDecoder] !== true)
          throw new errors.TypeError('ERR_INVALID_THIS', 'TextDecoder');
        return (this[kFlags] & CONVERTER_FLAGS_FATAL) === CONVERTER_FLAGS_FATAL;
      },

      get ignoreBOM() {
        if (this == null || this[kDecoder] !== true)
          throw new errors.TypeError('ERR_INVALID_THIS', 'TextDecoder');
        return (this[kFlags] & CONVERTER_FLAGS_IGNORE_BOM) ===
               CONVERTER_FLAGS_IGNORE_BOM;
      },

      [inspect](depth, opts) {
        if (this == null || this[kDecoder] !== true)
          throw new errors.TypeError('ERR_INVALID_THIS', 'TextDecoder');
        if (typeof depth === 'number' && depth < 0)
          return opts.stylize('[Object]', 'special');
        var ctor = getConstructorOf(this);
        var obj = Object.create({
          constructor: ctor === null ? TextDecoder : ctor
        });
        obj.encoding = this.encoding;
        obj.fatal = this.fatal;
        obj.ignoreBOM = this.ignoreBOM;
        if (opts.showHidden) {
          obj[kFlags] = this[kFlags];
          obj[kHandle] = this[kHandle];
        }
        // Lazy to avoid circular dependency
        return require('util').inspect(obj, opts);
      }
    }));
  Object.defineProperties(TextDecoder.prototype, {
    decode: { enumerable: true },
    [inspect]: { enumerable: false },
    [Symbol.toStringTag]: {
      configurable: true,
      value: 'TextDecoder'
    }
  });
}

module.exports = {
  getEncodingFromLabel,
  hasTextDecoder,
  TextDecoder,
  TextEncoder
};
