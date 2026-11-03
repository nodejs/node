'use strict';

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const { emitExperimentalWarning, kEmptyObject } = require('internal/util');
const { validateBoolean, validateObject } = require('internal/validators');
const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');
const { parseDocuments } = require('internal/yaml/parser');

emitExperimentalWarning('YAML');

let textDecoder;
function decode(input) {
  textDecoder ??= new (require('internal/encoding').TextDecoder)('utf-8');
  return textDecoder.decode(input);
}

/**
 * Parses a YAML stream. A stream with one document yields its value, a
 * stream with several documents yields an array of their values.
 * @param {string | ArrayBuffer | SharedArrayBuffer | ArrayBufferView} input
 * @param {{ throwIfInvalid?: boolean }} [options]
 * @returns {unknown}
 */
function parse(input, options = kEmptyObject) {
  let text;
  if (typeof input === 'string') {
    text = input;
  } else if (isArrayBufferView(input) || isAnyArrayBuffer(input)) {
    text = decode(input);
  } else {
    throw new ERR_INVALID_ARG_TYPE(
      'input', ['string', 'ArrayBuffer', 'SharedArrayBuffer', 'ArrayBufferView'], input);
  }
  validateObject(options, 'options');
  const { throwIfInvalid = true } = options;
  validateBoolean(throwIfInvalid, 'options.throwIfInvalid');
  const documents = parseDocuments(text, throwIfInvalid);
  if (documents === undefined) return undefined;
  if (documents.length === 0) return null;
  if (documents.length === 1) return documents[0];
  return documents;
}

module.exports = {
  parse,
};
