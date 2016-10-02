'use strict';

const Buffer = require('buffer').Buffer;
const normalizeEncoding = require('internal/util').normalizeEncoding;

if (process.binding('config').hasIntl) {

  const icu = process.binding('icu');

  // Maps the supported transcoding conversions. The top key is the from_enc,
  // the child key is the to_enc. The value is the transcoding function to.
  const conversions = {
    'ascii': {
      'latin1': (source) => {
        return Buffer.from(source);
      },
      'utf8': (source) => {
        return Buffer.from(source);
      },
      'utf16le': (source) => {
        return icu.convertToUcs2('us-ascii', source);
      }
    },
    'latin1': {
      'ascii': (source) => {
        return icu.convert('us-ascii', 'iso8859-1', source);
      },
      'utf8': (source) => {
        return icu.convert('utf-8', 'iso8859-1', source);
      },
      'utf16le': (source) => {
        return icu.convertToUcs2('iso8859-1', source);
      }
    },
    'utf8': {
      'ascii': (source) => {
        return icu.convert('us-ascii', 'utf-8', source);
      },
      'latin1': (source) => {
        return icu.convert('iso-8859-1', 'utf-8', source);
      },
      'utf16le': icu.convertToUcs2FromUtf8,
    },
    'utf16le': {
      'ascii': (source) => {
        if (source.length % 2 !== 0)
          throw new TypeError('Invalid UCS2 Buffer');
        return icu.convertFromUcs2('us-ascii', source);
      },
      'latin1': (source) => {
        if (source.length % 2 !== 0)
          throw new TypeError('Invalid UCS2 Buffer');
        return icu.convertFromUcs2('iso-8859-1', source);
      },
      'utf8': (source) => {
        if (source.length % 2 !== 0)
          throw new TypeError('Invalid UCS2 Buffer');
        return icu.convertToUtf8FromUcs2(source);
      }
    }
  };

  // Transcodes the Buffer from one encoding to another, returning a new
  // Buffer instance.
  exports.transcode = function transcode(source, from_enc, to_enc) {
    if (!source || !(source.buffer instanceof ArrayBuffer))
      throw new TypeError('"source" argument must be a Buffer');
    if (source.length === 0) return Buffer.alloc(0);

    from_enc = normalizeEncoding(from_enc) || from_enc;
    to_enc = normalizeEncoding(to_enc) || to_enc;

    if (from_enc === to_enc)
      return Buffer.from(source);

    const cnv_from = conversions[from_enc];

    if (cnv_from) {
      const cnv_to = cnv_from[to_enc];
      if (cnv_to)
        return cnv_to(source);
    }
    throw new TypeError(`Unsupported conversion: ${from_enc} to ${to_enc}`);
  };

  // Perform Unicode Normalization on the Buffer.
  exports.normalize = function normalize(buf, form, encoding) {
    if (!buf || !(buf.buffer instanceof ArrayBuffer))
      throw new TypeError('First argument must be a Buffer');
    encoding = normalizeEncoding(encoding);
    if (encoding === 'ascii')
      encoding == 'us-ascii';
    return icu.normalize(buf, encoding, String(form).toUpperCase());
  };

}
