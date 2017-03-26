'use strict';

const Buffer = require('buffer').Buffer;

function toBuf(str, encoding) {
  if (typeof str === 'string') {
    if (encoding === 'buffer' || !encoding)
      encoding = 'utf8';
    return Buffer.from(str, encoding);
  }
  return str;
}

module.exports = {
  toBuf,
  DEFAULT_ENCODING: 'buffer',
  kAlgorithm: Symbol('algorithm')
};
